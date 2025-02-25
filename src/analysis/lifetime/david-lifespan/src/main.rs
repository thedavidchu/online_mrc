/* Copyright belongs to Kia Shakiba. */
mod access;

use std::{
    collections::HashMap,
    hash::BuildHasherDefault,
    io,
    path::{Path, PathBuf},
};

use clap::Parser;
use dlv_list::{Index, VecList};
use nohash_hasher::NoHashHasher;
use parse_size::parse_size;

use kwik::{
    file::{
        FileReader,
        binary::{BinaryReader, SizedChunk},
    },
    fmt,
    plot::{Figure, Plot, box_plot::BoxPlot},
    progress::{Progress, Tag},
};

use crate::access::Access;

#[derive(Parser)]
#[command(author, version, about, long_about = None)]
struct Args {
    #[arg(short, long)]
    path: PathBuf,

    #[arg(short = 's', long)]
    max_size: String,

    #[arg(short, long, default_value_t = 10)]
    num_steps: u32,

    #[arg(short, long)]
    output: PathBuf,
}

struct Object {
    first_access: u64,
    key: u64,
    size: u32,
}

#[allow(dead_code)]
fn linspace(max_size: u64, max_num_steps: u64) -> Vec<u64> {
    let mut space = Vec::<u64>::new();
    let step = max_size / max_num_steps as u64;
    for cache_size in (step..=max_size).step_by(step as usize) {
        if cache_size == 0 {
            println!("Get cache size of 0");
            continue;
        }
        space.push(cache_size);
    }
    space
}

#[allow(dead_code)]
fn logspace(max_size: u64, max_num_steps: u64) -> Vec<u64> {
    let mut space = Vec::<u64>::new();
    for i in 1..=max_num_steps {
        let cache_size = max_size / (1 << (max_num_steps - i));
        if cache_size == 0 {
            println!("Get cache size of 0");
            continue;
        }
        space.push(cache_size)
    }
    space
}

fn main() -> anyhow::Result<()> {
    let args = Args::parse();

    let max_size = parse_size(&args.max_size)?;

    println!("{}\n", args.path.to_str().unwrap_or_default());

    let mut sized_lifespans = Vec::<(String, Vec<u64>)>::new();

    sized_lifespans.push(("TTL (all)".to_string(), get_ttls(&args.path)?));

    for cache_size in logspace(max_size, args.num_steps as u64) {
        println!("Cache size: {}", fmt::memory(cache_size, Some(2)));

        let reader = BinaryReader::<Access>::from_path(&args.path)?;

        let mut progress = Progress::new(reader.size())
            .with_tag(Tag::Tps)
            .with_tag(Tag::Eta)
            .with_tag(Tag::Time);

        let mut current_size = 0u64;

        let mut map: HashMap<u64, Index<Object>, BuildHasherDefault<NoHashHasher<u64>>> =
            HashMap::with_hasher(BuildHasherDefault::default());

        let mut stack = VecList::<Object>::new();
        let mut lifespans = Vec::<u64>::new();

        for access in reader {
            progress.tick(Access::size());

            if !access.is_valid_read_through() {
                continue;
            }

            if access.size as u64 > cache_size {
                continue;
            }

            match map.get(&access.key) {
                Some(index) => {
                    let object = stack.remove(*index).unwrap();
                    let new_index = stack.push_front(object);

                    map.insert(access.key, new_index);
                }

                None => {
                    while current_size > cache_size {
                        let object = stack.pop_back().unwrap();

                        map.remove(&object.key);
                        current_size -= object.size as u64;

                        lifespans.push(access.timestamp - object.first_access);
                    }

                    let object = Object::new(&access);
                    let index = stack.push_front(object);

                    map.insert(access.key, index);
                    current_size += access.size as u64;
                }
            }
        }

        let label = fmt::memory(cache_size, Some(2));
        sized_lifespans.push((label, lifespans));
    }

    plot(&args.output, &sized_lifespans)?;

    Ok(())
}

fn get_ttls(path: &PathBuf) -> anyhow::Result<Vec<u64>> {
    println!("Time-to-Live");
    let reader = BinaryReader::<Access>::from_path(&path)?;
    let mut progress = Progress::new(reader.size())
        .with_tag(Tag::Tps)
        .with_tag(Tag::Eta)
        .with_tag(Tag::Time);

    let mut lifespans = Vec::<u64>::new();
    for access in reader {
        progress.tick(Access::size());
        // The Twitter traces only have TTLs with the PUT requests, so
        // we must not ignore those ones.
        if let Some(ttl) = access.ttl {
            lifespans.push(ttl as u64 * 1000u64);
        }
    }
    Ok(lifespans)
}

fn plot<P>(path: P, sized_lifespans: &[(String, Vec<u64>)]) -> io::Result<()>
where
    P: AsRef<Path>,
{
    let mut plot = BoxPlot::default()
        .with_x_label("Cache size")
        .with_y_label("Lifetime (hrs)")
        .with_format_y_log(true);

    for (label, lifespans) in sized_lifespans {
        for &lifespan in lifespans {
            plot.add(&label, lifespan as f64 / 3_600_000 as f64);
        }
    }

    let mut figure = Figure::default();

    figure.add(plot);
    figure.save(&path)?;

    println!(
        "\nPlot saved to <{}>",
        path.as_ref().to_str().unwrap_or_default()
    );

    Ok(())
}

impl Object {
    fn new(access: &Access) -> Self {
        Object {
            first_access: access.timestamp,
            key: access.key,
            size: access.size,
        }
    }
}

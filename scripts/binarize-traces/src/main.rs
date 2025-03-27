mod access;
mod convert;

use std::{
    fs,
    path::{Path, PathBuf},
};

use clap::{Parser, ValueEnum};

use kwik::{
    file::{FileWriter, binary::BinaryWriter},
    progress::{Progress, Tag},
};

use crate::{
    access::Access,
    convert::{AccessType::Supported, Convert, Twitter},
};

#[derive(Parser)]
#[command(author, version, about, long_about = None)]
struct Args {
    #[arg(short, long, default_value = ".")]
    dir: PathBuf,

    #[arg(short, long)]
    name: String,

    #[arg(short, long)]
    format: Format,
}

#[derive(Clone, ValueEnum)]
enum Format {
    Twitter,
}

fn main() {
    let args = Args::parse();
    let files = get_files(&args.dir, &args.name, &args.format);

    println!("Found {} file(s)", files.len());

    if !files.is_empty() {
        println!();
    }

    let mut output = args.dir.to_path_buf();
    output.push(format!("{}.bin", args.name));

    let mut writer = BinaryWriter::<Access>::from_path(output).unwrap();

    for file in &files {
        println!("{}", file);

        let converter = get_converter(&args.format, file);

        if converter.file_size() == 0 {
            continue;
        }

        let mut progress = Progress::new(converter.file_size())
            .with_tag(Tag::Tps)
            .with_tag(Tag::Eta)
            .with_tag(Tag::Time);

        for (access_type, row_size) in converter {
            if let Supported(access) = access_type {
                writer.write_chunk(&access).unwrap();
            }

            progress.tick(row_size);
        }
    }
}

fn get_converter(format: &Format, file_path: &str) -> Box<dyn Convert> {
    match format {
        Format::Twitter => Box::new(Twitter::new(file_path)),
    }
}

fn get_files<P>(dir_path: P, name: &str, format: &Format) -> Vec<String>
where
    P: AsRef<Path>,
{
    let mut files = Vec::<String>::new();
    let mut count = 0;

    let mut path = dir_path.as_ref().to_path_buf();

    let ext = get_file_extension(format);
    path.push(format!("{}.{}.{ext}", name, count));

    while fs::metadata(&path).is_ok() {
        files.push(path.to_string_lossy().into_owned());

        count += 1;
        path.set_file_name(format!("{}.{}.{ext}", name, count));
    }

    files
}

fn get_file_extension(format: &Format) -> &'static str {
    match format {
        // Format::Oracle => "bin",
        _ => "csv",
    }
}

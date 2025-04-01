use std::{io, path::Path};

use fasthash::murmur3;

pub use kwik::file::{FileReader, text::TextReader};

use crate::access::{ClientId, KeySize};
pub use crate::{
    access::{Access, Command, Key, Timestamp, Ttl, ValueSize},
    convert::{
        AccessType,
        AccessType::{Supported, Unsupported},
        Convert, ConvertRow,
    },
};

pub struct Twitter {
    reader: TextReader,
    size: u64,
}

struct TwitterRow {
    timestamp: Timestamp,
    key: String,
    ksize: KeySize,
    vsize: ValueSize,
    cid: ClientId,
    command: String,
    ttl: Ttl,

    row_size: usize,
}

impl Convert for Twitter {
    fn new<P>(path: P) -> Self
    where
        Self: Sized,
        P: AsRef<Path>,
    {
        let reader = TextReader::from_path(path).unwrap();
        let size = reader.size();

        Twitter { reader, size }
    }

    fn read_row(&mut self) -> io::Result<(AccessType, usize)> {
        self.reader.read_line().map(|line| {
            let twitter_row = line.as_str().try_into().expect("Could not parse row.");

            let access_type = match parse_command(&twitter_row) {
                Some(command) => Supported(Access {
                    timestamp: twitter_row.timestamp,
                    command,
                    key: parse_key(&twitter_row),
                    ksize: twitter_row.ksize,
                    vsize: twitter_row.vsize,
                    cid: twitter_row.cid,
                    ttl: parse_ttl(&twitter_row),
                }),

                None => Unsupported,
            };

            (access_type, twitter_row.size())
        })
    }

    fn file_size(&self) -> u64 {
        self.size
    }
}

impl TryFrom<&str> for TwitterRow {
    type Error = io::Error;

    fn try_from(value: &str) -> Result<Self, Self::Error> {
        let tokens = value.split(',').collect::<Vec<_>>();

        let timestamp = tokens[0]
            .parse::<Timestamp>()
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, "Invalid timestamp."))?;

        let key = tokens[1..(tokens.len() - 5)].join(",").to_string();
        let ksize = tokens[tokens.len() - 5]
            .parse::<KeySize>()
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, "Invalid size."))?;
        let vsize = tokens[tokens.len() - 4]
            .parse::<ValueSize>()
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, "Invalid size."))?;
        let cid = tokens[tokens.len() - 3]
            .parse::<ClientId>()
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, "Invalid size."))?;
        let command = tokens[tokens.len() - 2].to_string();
        let ttl = tokens[tokens.len() - 1]
            .parse::<Ttl>()
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, "Invalid ttl."))?;

        let twitter_row = TwitterRow {
            timestamp: timestamp * 1000,
            key,
            ksize,
            vsize,
            cid,
            command,
            ttl,

            row_size: value.len() + 1,
        };

        Ok(twitter_row)
    }
}

impl ConvertRow for TwitterRow {
    fn size(&self) -> usize {
        self.row_size
    }
}

fn parse_command(twitter_row: &TwitterRow) -> Option<Command> {
    match twitter_row.command.as_str() {
        "get" => Some(Command::Get),
        "set" => Some(Command::Set),
        "add" => Some(Command::Add),
        "replace" => Some(Command::Replace),
        "append" => Some(Command::Append),
        "prepend" => Some(Command::Prepend),
        "incr" => Some(Command::Incr),
        "decr" => Some(Command::Decr),
        "gets" => Some(Command::Gets),
        "cas" => Some(Command::Cas),
        "delete" => Some(Command::Delete),

        _ => None,
    }
}

fn parse_key(twitter_row: &TwitterRow) -> Key {
    hash(&twitter_row.key) as Key
}

fn parse_ttl(twitter_row: &TwitterRow) -> Option<Ttl> {
    match twitter_row.ttl {
        0 => None,
        ttl => Some(ttl),
    }
}

fn hash(key_string: &str) -> u128 {
    murmur3::hash128(key_string)
}

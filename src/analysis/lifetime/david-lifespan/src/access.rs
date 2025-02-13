/* Copyright belongs to Kia Shakiba. */
use byteorder::{LittleEndian, ReadBytesExt};
use kwik::file::binary::{ReadChunk, SizedChunk};
use std::io::{self, Cursor};

#[allow(dead_code)]
#[derive(Debug)]
pub struct Access {
    pub timestamp: u64,
    pub command: Command,
    pub key: u64,
    pub size: u32,
    pub ttl: Option<u32>,
}

#[derive(Debug, PartialEq, Eq)]
pub enum Command {
    Get,
    Set,
}

impl Access {
    pub fn is_valid_read_through(&self) -> bool {
        self.command == Command::Get && self.size > 0
    }
}

impl SizedChunk for Access {
    fn size() -> usize {
        25
    }
}

impl ReadChunk for Access {
    fn from_chunk(buf: &[u8]) -> io::Result<Self> {
        let mut rdr = Cursor::new(buf);

        let timestamp = rdr.read_u64::<LittleEndian>()?;
        let command_byte = rdr.read_u8()?;

        let command = match command_byte {
            0 => Command::Get,
            1 => Command::Set,

            _ => {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    "Invalid command byte.",
                ))
            }
        };

        let key = rdr.read_u64::<LittleEndian>()?;
        let size = rdr.read_u32::<LittleEndian>()?;

        let ttl = match rdr.read_u32::<LittleEndian>()? {
            0 => None,
            value => Some(value),
        };

        let access = Access {
            timestamp,
            command,
            key,
            size,
            ttl,
        };

        Ok(access)
    }
}

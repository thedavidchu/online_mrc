use byteorder::{LittleEndian, ReadBytesExt};
use kwik::file::binary::{ReadChunk, SizedChunk, WriteChunk};
use std::io::{self, Cursor};

// With millisecond precision, this can represent up to around 49 days,
// which is good enough for the Twitter traces.
pub type Timestamp = u32;
pub type Key = u64;
pub type KeySize = u8;
pub type ValueSize = u32;
pub type ClientId = u32;
pub type Ttl = u32;

#[derive(Debug, PartialEq, Clone)]
pub enum Command {
    Nop,
    Get,
    Gets,
    Set,
    Add,
    Cas,
    Replace,
    Append,
    Prepend,
    Delete,
    Incr,
    Decr,
    Read,
    Write,
    Update,
    Invalid,
}

#[derive(Debug, Clone)]
pub struct Access {
    pub timestamp: Timestamp,
    pub command: Command,
    pub key: Key,
    pub ksize: KeySize,
    pub vsize: ValueSize,
    pub cid: ClientId,
    pub ttl: Option<Ttl>,
}

impl SizedChunk for Access {
    fn size() -> usize {
        24
    }
}

impl ReadChunk for Access {
    fn from_chunk(buf: &[u8]) -> io::Result<Self> {
        let mut rdr = Cursor::new(buf);

        let timestamp = rdr.read_u32::<LittleEndian>()?;
        let key = rdr.read_u64::<LittleEndian>()?;
        let key_value_size = rdr.read_u32::<LittleEndian>()?;
        let op_ttl = rdr.read_u32::<LittleEndian>()?;
        let cid = rdr.read_u32::<LittleEndian>()?;

        let ksize = (key_value_size >> 22) as u8;
        let vsize = key_value_size & 0x003F_FFFF;
        let op = (op_ttl >> 24) as u8;
        let ttl = match op_ttl & 0x00FF_FFFF {
            0 => None,
            value => Some(value),
        };

        let access = Access {
            timestamp,
            command: Command::from_byte(op).unwrap(),
            key,
            ksize,
            vsize,
            cid,
            ttl,
        };

        Ok(access)
    }
}

// struct {
//  timestamp: u32,
//  key: u64,
//  ksize: u32:8
//  vsize: u32:24
//  op: u32:8
//  ttl: u32:24
//  cid: u32
// }
impl WriteChunk for Access {
    fn as_chunk(&self, buf: &mut Vec<u8>) -> io::Result<()> {
        buf.extend_from_slice(&self.timestamp.to_le_bytes());
        buf.extend_from_slice(&self.key.to_le_bytes());

        assert!((self.ksize as u32) < ((1 as u32) << 10));
        assert!(self.vsize < 1 << 22);
        let key_value_size: u32 = (self.ksize as u32) << 22 | self.vsize;
        buf.extend_from_slice(&key_value_size.to_le_bytes());
        assert!((self.command.as_byte() as u32) < ((1 as u32) << 8));
        let ttl = if self.ttl.unwrap_or(0) < 1 << 24 {
            self.ttl.unwrap_or(0)
        } else {
            println!("warning: oversized TTL {}", self.ttl.unwrap_or(0));
            1 << 24 - 1
        };
        let op_ttl: u32 = (self.command.as_byte() as u32) << 24 | ttl;
        buf.extend_from_slice(&op_ttl.to_le_bytes());
        buf.extend_from_slice(&self.cid.to_le_bytes());

        Ok(())
    }
}

impl Command {
    fn from_byte(byte: u8) -> io::Result<Self> {
        match byte {
            0 => Ok(Command::Nop),
            1 => Ok(Command::Get),
            2 => Ok(Command::Gets),
            3 => Ok(Command::Set),
            4 => Ok(Command::Add),
            5 => Ok(Command::Cas),
            6 => Ok(Command::Replace),
            7 => Ok(Command::Append),
            8 => Ok(Command::Prepend),
            9 => Ok(Command::Delete),
            10 => Ok(Command::Incr),
            11 => Ok(Command::Decr),
            12 => Ok(Command::Read),
            13 => Ok(Command::Write),
            14 => Ok(Command::Update),
            255 => Ok(Command::Invalid),
            _ => Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "Invalid command byte.",
            )),
        }
    }

    fn as_byte(&self) -> u8 {
        match self {
            Command::Nop => 0,
            Command::Get => 1,
            Command::Gets => 2,
            Command::Set => 3,
            Command::Add => 4,
            Command::Cas => 5,
            Command::Replace => 6,
            Command::Append => 7,
            Command::Prepend => 8,
            Command::Delete => 9,
            Command::Incr => 10,
            Command::Decr => 11,
            Command::Read => 12,
            Command::Write => 13,
            Command::Update => 14,
            Command::Invalid => 255,
        }
    }
}

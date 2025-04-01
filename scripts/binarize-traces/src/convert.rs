mod twitter;

use std::{io, path::Path};

pub enum AccessType {
    Supported(Access),
    Unsupported,
}

pub trait Convert {
    fn new<P>(path: P) -> Self
    where
        Self: Sized,
        P: AsRef<Path>;

    fn read_row(&mut self) -> io::Result<(AccessType, usize)>;
    fn file_size(&self) -> u64;
}

pub trait ConvertRow {
    fn size(&self) -> usize;
}

impl Iterator for dyn Convert {
    type Item = (AccessType, usize);

    fn next(&mut self) -> Option<Self::Item> {
        match self.read_row() {
            Ok(row) => Some(row),
            Err(err) if err.kind() == io::ErrorKind::UnexpectedEof => None,
            Err(err) => panic!("{err:?}"),
        }
    }
}

pub use crate::convert::twitter::*;

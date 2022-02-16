pub struct Block<'a> {
    pub validator: &'a [u8],
    pub parent: &'a [u8],
    pub timestamp: u64,
    pub height: u64,
    pub records: &'a [u8],
    pub signature: &'a [u8],
}

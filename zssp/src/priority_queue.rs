
/// Is a Donald Knuth minheap, which is extremely fast and memory efficient.
pub struct EventQueue {
	heap: Vec<(i64, u64)>
}

impl EventQueue {
	pub fn new() -> Self {
		Self {
			heap: Vec::new(),
		}
	}
	/// Pops a single event from the queue if one exists to be run past the current time
	pub fn pump(&mut self, current_time: i64) -> Option<(i64, u64)> {
		if self.heap.len() > 0 {
			if self.heap[0].0 <= current_time {
				let ret = self.heap.swap_remove(0);
				let mut parent = 0;
				while 2*parent < self.heap.len() {
					let child0 = 2*parent;
					let child1 = child0 + 1;
					let child_min = if child1 < self.heap.len() && self.heap[child1].0 < self.heap[child0].0 {
						child1
					} else {
						child0
					};
					if self.heap[child_min].0 < self.heap[parent].0 {
						self.heap.swap(parent, child_min);
						parent = child_min;
					} else {
						break;
					}
				}
				return Some(ret);
			}
		}
		None
	}

	/// Pushes an event onto the queue with the given timestamp
	pub fn push(&mut self, timestamp: i64, id: u64) {
		let mut idx = self.heap.len();
		self.heap.push((timestamp, id));
		while idx > 0 {
			let parent = idx/2;
			if self.heap[parent].0 > self.heap[idx].0 {
				self.heap.swap(parent, idx);
			}
			idx = parent;
		}
	}
}

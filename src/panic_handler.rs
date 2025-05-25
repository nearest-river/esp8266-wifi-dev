
use core::{
  panic::PanicInfo,
  sync::atomic::{
    self,
    Ordering
  }
};


#[inline(never)]
#[panic_handler]
#[no_mangle]
fn panic(_info: &PanicInfo) -> ! {
  loop {
    atomic::compiler_fence(Ordering::SeqCst);
  }
}




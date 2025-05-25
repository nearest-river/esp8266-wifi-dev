#![no_std]
#![no_main]
#![feature(error_in_core)]
#![feature(asm_experimental_arch)]

#[allow(unused)]
mod bindings;
mod panic_handler;
mod exception_vectors;

#[allow(unused_imports)]
use bindings as _;
use core::fmt::Write as _;
use esp8266_hal::{
  prelude::*,
  target::Peripherals
};

extern "C" { fn malloc(size: usize)-> *mut u8; }


#[esp8266_hal::entry]
fn main()-> ! {
  let dp=Peripherals::take().unwrap();
  let pins=dp.GPIO.split();
  let mut stdout=dp.UART0.serial(pins.gpio1.into_uart(),pins.gpio3.into_uart());

  writeln!(stdout,"right before loop\n\n\n\n\n\n\n").unwrap();
  let _ptr=unsafe { malloc(1024) };


  loop {
    unsafe {
      bindings::user_interface::system_get_boot_mode();
    }



    writeln!(stdout,"tick\n").unwrap();
  }
}






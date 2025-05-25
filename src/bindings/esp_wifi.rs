
#[repr(C)]
pub enum AuthMode {
  Open=0,
  Wep,
  WpaPsk,
  Wpa2Psk,
  WpaWpa2Psk,
  Max
}

extern "C" {
  pub fn wifi_get_opmode()-> u8;
  pub fn wifi_set_opmode(opmode: u8);
}


#[repr(C)]
pub struct IpInfo {
  xd: u8,
  // ip: Ip4Addr,
  // netmask: Ip4Addr,
  // gw: Ip4Addr
}

extern "C" {
  pub fn wifi_get_ip_info(if_index: u8,info: *mut IpInfo)-> bool;
  pub fn wifi_set_ip_info(if_index: u8,info: *mut IpInfo)-> bool;
  pub fn wifi_get_macaddr(if_index: u8,macaddr: *mut u8)-> bool;
  pub fn wifi_set_macaddr(if_index: u8,macaddr: *mut u8)-> bool;

  pub fn wifi_get_channel()-> u8;
  pub fn wifi_set_channel(channel: u8)-> bool;
}












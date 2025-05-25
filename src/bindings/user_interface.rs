
#[repr(C)]
pub enum DHCPStatus {
  Stopped,
  Started
}

extern "C" {
  pub fn system_get_boot_version()-> u8;
  pub fn system_get_userbin_addr()-> u32;
  pub fn system_get_boot_mode()-> u8;
  pub fn system_restart_enhance(bin_type: u8,bin_addr: u32)-> bool;
  pub fn system_upgrade_userbin_set(userbin: u8)-> bool;
  pub fn system_upgrade_userbin_check()-> u8;
  pub fn system_upgrade_flag_set(flag: u8)-> bool;
  pub fn system_upgrade_flag_check()-> u8;
  pub fn system_upgrade_reboot()-> bool;
  pub fn wifi_station_dhcpc_start()-> bool;
  pub fn wifi_station_dhcpc_stop()-> bool;
  pub fn wifi_station_dhcpc_status()-> DHCPStatus;
}












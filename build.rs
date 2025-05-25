use std::{
  fs,
  io,
  env,
  ffi::OsStr,
  path::Path,
  error::Error,
  process::Command
};


static LIB_PATH: &str="./lib";
static FFI_DIR: &str="./src/ffi";
static FFI_BUILD: &str="make";
static XTENSA_AR: &str="xtensa-lx106-elf-ar";
static ENV_VARS_REMOVE: [&str;2]=["TARGET","TOOLCHAIN"];
static XTENSA_CARGO_PATH: &str=concat!(env!("RUSTUP_HOME"),"toolchains/esp/bin/cargo");
static ENV_VARS_REASSIGN: [(&str,&str);1]=[
  ("CARGO",XTENSA_CARGO_PATH),
];


fn main()-> Result<(),Box<dyn Error>> {
  init();

  build_ffi()?;
  link()?;
  Ok(())
}

fn init() {
  println!("cargo:rerun-if-changed=./build.rs");
  println!("cargo:rerun-if-changed={FFI_DIR}/build/libesp8266-sys-core.a");


  ENV_VARS_REMOVE
  .iter()
  .for_each(|key| env::remove_var(key));

  ENV_VARS_REASSIGN
  .iter()
  .for_each(|(key,val)| env::set_var(key,val));
}

fn link()-> io::Result<()> {
  for entry in fs::read_dir(LIB_PATH)?.flatten() {
    let path=entry.path();
    if !path.is_file() || !path.ends_with(".remove") {
      continue;
    }

    let objects=parse_remove_config(&path)?;
    let mut archive_path=path;
    archive_path.set_extension("a");

    remove_objects_from_archive(archive_path,&objects)?;
  }


  println!("cargo:rustc-link-search={LIB_PATH}");
  println!("cargo:rustc-link-lib={}","main");
  println!("cargo:rustc-link-lib={}","net80211");
  println!("cargo:rustc-link-lib={}","wpa");
  println!("cargo:rustc-link-lib={}","gcc");
  println!("cargo:rustc-link-lib={}","phy");
  println!("cargo:rustc-link-lib={}","pp");
  println!("cargo:rustc-link-lib={}","g");
  println!("cargo:rustc-link-lib={}","c");
  println!("cargo:rustc-link-lib={}","m");
  println!("cargo:rustc-link-lib={}","esp8266-sys-core");
  Ok(())
}

fn build_ffi()-> io::Result<()> {
  let process_handle=Command::new(FFI_BUILD)
  .current_dir(FFI_DIR)
  .status()?;

  if !process_handle.success() {
    let code=process_handle.code()
    .unwrap_or(-1);
    let err=io::Error::from_raw_os_error(code);

    return Err(err);
  }

  Ok(())
}


fn remove_objects_from_archive<A: AsRef<OsStr>,O: AsRef<OsStr>>(archive: A,objects: &[O])-> io::Result<()> {
  let process_handle=Command::new(XTENSA_AR)
  .arg("d")
  .arg(archive)
  .args(objects)
  .status()?;

  if !process_handle.success() {
    let code=process_handle.code()
    .unwrap_or(-1);
    let err=io::Error::from_raw_os_error(code);

    return Err(err);
  }

  Ok(())
}

fn parse_remove_config<P: AsRef<Path>>(path: P)-> io::Result<Vec<String>> {
  let objects=fs::read_to_string(path)?
  .lines()
  .filter(|line| !line.starts_with('#'))
  .map(str::to_owned)
  .collect::<Vec<_>>();

  Ok(objects)
}





















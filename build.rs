//! Stamps the Windows exe with its icon and version. The version comes from the
//! release tag via AC_VERSION; an untagged build reports 0.0.0-dev and never self-updates.
fn main() {
    println!("cargo:rerun-if-env-changed=AC_VERSION");
    println!("cargo:rerun-if-changed=assets/icon.ico");
    if std::env::var("CARGO_CFG_TARGET_OS").as_deref() != Ok("windows") {
        return;
    }
    let version = std::env::var("AC_VERSION").unwrap_or_else(|_| "0.0.0-dev".into());
    let nums: Vec<u64> = version.split('.').filter_map(|p| p.parse().ok()).collect();
    let part = |i: usize| nums.get(i).copied().unwrap_or(0) & 0xFFFF;
    let packed = (part(0) << 48) | (part(1) << 32) | (part(2) << 16);

    let mut res = winresource::WindowsResource::new();
    res.set_icon("assets/icon.ico")
        .set("ProductName", "AzerothCore")
        .set("FileDescription", "AzerothCore")
        .set("FileVersion", &version)
        .set("ProductVersion", &version)
        .set_version_info(winresource::VersionInfo::FILEVERSION, packed)
        .set_version_info(winresource::VersionInfo::PRODUCTVERSION, packed);
    res.compile().expect("failed to compile Windows resources");
}

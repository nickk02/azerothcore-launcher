//! Types the login into the game's login screen. Windows only: the macOS app
//! keeps the login in the Keychain and fills its own fields.

#[cfg(windows)]
pub fn type_login(account: &str, password: &str) -> bool {
    use windows_sys::Win32::UI::Input::KeyboardAndMouse::*;
    use std::{thread::sleep, time::Duration};

    fn key(vk: u16, scan: u16, flags: u32) -> INPUT {
        INPUT {
            r#type: INPUT_KEYBOARD,
            Anonymous: INPUT_0 { ki: KEYBDINPUT { wVk: vk, wScan: scan, dwFlags: flags, time: 0, dwExtraInfo: 0 } },
        }
    }
    fn send(inputs: &[INPUT]) -> bool {
        unsafe { SendInput(inputs.len() as u32, inputs.as_ptr(), std::mem::size_of::<INPUT>() as i32) == inputs.len() as u32 }
    }
    fn text(s: &str) -> bool {
        s.encode_utf16().all(|ch| {
            send(&[key(0, ch, KEYEVENTF_UNICODE), key(0, ch, KEYEVENTF_UNICODE | KEYEVENTF_KEYUP)])
        })
    }

    // Give the client's login screen time to draw and take focus.
    sleep(Duration::from_millis(2500));
    text(account) && send(&[key(VK_TAB, 0, 0), key(VK_TAB, 0, KEYEVENTF_KEYUP)]) && text(password)
}

#[cfg(not(windows))]
pub fn type_login(_account: &str, _password: &str) -> bool {
    false
}

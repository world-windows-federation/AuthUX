# AuthUX

**AuthUX** is a project based on [the ConsoleLogon decompilation](//github.com/explorer7-team/ConsoleLogon) which aims to replicate the Windows 7 logon screen.

> [!WARNING]
> **THIS PROJECT IS IN EARLY DEVELOPMENT AND MIGHT BE UNSTABLE. Use at your own risk. We are not liable for any damages that may or may not occur.**
>
> **PLEASE DISABLE BITLOCKER BEFORE USING THIS**
> 
> **PLEASE HAVE A USB DRIVE WITH A WINDOWS INSTALLER READY BEFORE HAND TO BE ABLE TO RECOVER YOUR WINDOWS INSTALL IN THE EVENT OF AUTHUX BRICKING YOUR SYSTEM**

## Manual Installation

> [!WARNING]
> **To ensure a stable experience, you should ensure that you install a version of AuthUX compiled for your version of Windows.**

In order to install AuthUX, you need to write to protected registry keys which you ordinarily do not have permission to write to. An easy way to do this is to [download RunTI](https://github.com/aubymori/RunTI) and use it to run a Command Prompt (`cmd.exe`) window as the special internal TrustedInstaller user account. The keys which you must write to are:

- `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LogonUX\DllPath`
- `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LockScreenHost\DllPath`
- `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.BlockedShutdownResolverUX\DllPath`

If you ran a TrustedInstaller `cmd.exe` instance as per the above recommendation, then the following commands will override those keys for you. Note that these commands presume the location of your AuthUX binaries to be `%SystemRoot%\System32\AuthUX.dll` (so in other words, you should move the files to System32; they will not conflict with anything):

```cmd
reg add HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LogonUX /v DllPath /t REG_SZ /d %SystemRoot%\System32\AuthUX.dll /f
reg add HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LockScreenHost /v DllPath /t REG_SZ /d %SystemRoot%\System32\AuthUX.dll /f
reg add HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.BlockedShutdownResolverUX /v DllPath /t REG_SZ /d %SystemRoot%\System32\AuthUX.dll /f
```

If these steps have been followed properly, then the logon screen will immediately start using AuthUX.

> [!NOTE]
> **An easy and safe way you can test whether or not AuthUX is working is by pressing CTRL+ALT+DEL on your keyboard to display the security options screen.** If an error occurred during setup, then the security options screen will likely fail to display and you will be taken back to the desktop, where you should immediately revert the values. [See the Uninstallation section of this document for more information.](#uninstallation)

## Manual Uninstallation

In order to restore the official Windows 10 logon screen, you need to revert the above registry key paths as outlined in the [Installation section](#installation).

The default values are displayed in the below table:

| **Windows.Internal.UI.Logon.Controller.LogonUX**        | `%SystemRoot%\system32\Windows.UI.Logon.dll` |
|---------------------------------------------------------|----------------------------------------------|
| **Windows.Internal.UI.Logon.Controller.LockScreenHost** | `%SystemRoot%\system32\logoncontroller.dll`  |
| **Windows.Internal.UI.Logon.Controller.BlockedShutdownResolverUX** | `%SystemRoot%\system32\Windows.UI.BlockedShutdown.dll` |

You can use the following commands to restore these default values, once again by opening a TrustedInstaller `cmd.exe` window via [RunTI](https://github.com/aubymori/RunTI):

```cmd
reg add HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LogonUX /v DllPath /t REG_SZ /d %SystemRoot%\system32\Windows.UI.Logon.dll /f
reg add HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LockScreenHost /v DllPath /t REG_SZ /d %SystemRoot%\system32\logoncontroller.dll /f
reg add HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.BlockedShutdownResolverUX /v DllPath /t REG_SZ /d %SystemRoot%\system32\Windows.UI.BlockedShutdown.dll /f
```

Following this modification, the logon screen will immediately revert to the vanilla experience.

## Building

**You need the Windows SDK for your target version. It is recommended to install a modern version of Visual Studio, such as Visual Studio 2022.**

> [!WARNING]
> Note: You might encounter issues on earlier versions of the MSVC toolset. Version 14.42 is tested and known to work.
>
> Additionally, you may encounter issues with the Windows Implementation Library (WIL) NuGet packages. If this is the case, try to uninstall and reinstall the packages.

This repository relies on Git submodules for the DirectUI library (dui70). In order to retrieve these files which are necessary for building, you must clone with submodules or fetch them.

To clone the repository with submodules, run:

```cmd
git clone --recurse-submodules https://github.com/explorer7-team/AuthUX.git
```

To fetch submodules after cloning the repository, run:

```cmd
git submodule update --init --recursive
```

Once you have cloned the repository, you must configure the build version in [`sdk/inc/version.h`](/sdk/inc/version.h). Change the line which defines `CONSOLELOGON_FOR` to one of the predefined constants. For example, the default value is:

```cpp
#ifndef CONSOLELOGON_FOR
#define CONSOLELOGON_FOR CONSOLELOGON_FOR_VB
#endif
```

which configures the build for the Windows 10 Vibranium (VB) development semester, which corresponds to all builds in the 1904x build range, or 2004 through 22H2.

The following table should aid you in determining which value to use:

| **Semester Name** | **Abbreviation** | **Build Number**                  | **Version Number**           | **Operating System Family**                                     |
|-------------------|------------------|-----------------------------------|------------------------------|-----------------------------------------------------------------|
| \*_Redstone 2_      | _RS2_            | _15063_                           | _1703_                       | _Windows 10 (Creators Update)_                                  |
| \*_Redstone 3_      | _RS3_            | _16299_                           | _1709_                       | _Windows 10 (Fall Creators Update)_                             |
| \*_Redstone 4_      | _RS4_            | _17134_                           | _1803_                       | _Windows 10_                                                    |
| Redstone 5        | RS5              | 17763                             | 1809                         | Windows 10                                                      |
| -                 | 19H1             | 18362, 18363                      | 1903, 1909                   | Windows 10                                                      |
| Vibranium         | VB               | 19041, 19042, 19043, 19044, 19045 | 2004, 20H2, 21H1, 21H2, 22H2 | Windows 10                                                      |
| Manganese         | MN               | 19551~19624                       | 2004                         | Windows 10 Insider Preview, Windows Server 2022 Insider Preview |
| Iron              | FE               | 20348                             | 21H2                         | Windows Server 2022                                             |
| Cobalt            | CO               | 22000                             | 21H2                         | Windows 11                                                      |
| Nickel            | NI               | 22621, 22631                      | 22H2                         | Windows 11                                                      |
| Zinc              | ZN               | 25246~25393                       | 23H2                         | Windows 11 Insider Preview                                      |
| Germanium         | GE               | 26100, 26120, 26200               | 24H2                         | Windows 11                                                      |

*Italicised entries are not currently supported. The minimum supported is currently RS5.*

Once that's all configured, you should just be able to compile the project by just building the solution. You should get a resulting `AuthUX.dll` file, [which you can use to install with the same installation instructions as above.](#installation)

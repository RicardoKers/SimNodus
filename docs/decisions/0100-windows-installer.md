# ADR 0100: Windows installer with Inno Setup, per user by default

Date: 2026-10-19. Status: accepted (owner's choices of 2026-10-19: Inno Setup,
and the install scope chosen in the installer; task SN-152).

## Context

SimNodus ships as a portable zip since 0.1: unzip and run, nothing installed.
Classrooms also need an installer: a Start menu entry, double-click on `.snx`
files, an uninstaller, and installation by the lab's IT staff. Students often
have no administrator rights; lab PCs are prepared by an administrator.

## Decision

- **Inno Setup 6** builds `SimNodus-<version>-windows-x64-setup.exe` from the
  same staging folder as the zip (`tools/package_windows.py --installer`,
  script `resources/package/simnodus.iss`). It is free, its license is
  permissive, it is not linked into SimNodus, and GitHub's Windows runners have
  it. The zip stays: the portable form is still the quickest for a test.
- **Scope:** per user without administrator rights by default
  (`PrivilegesRequired=lowest`); the first dialog lets an administrator install
  for all users in Program Files (`PrivilegesRequiredOverridesAllowed=dialog`).
  Silent installation (`/VERYSILENT /ALLUSERS` or `/CURRENTUSER`) serves
  scripted lab deployment.
- **What it does:** Start menu entry, optional desktop icon, `.snx` association
  (a task, on by default; opening a project never runs code, PRJ-03),
  uninstaller; English and Brazilian Portuguese dialogs. Settings in
  `%APPDATA%\SimNodus` are kept on uninstall.
- **Check:** each `--installer` build installs the setup silently into a
  temporary folder for the current user (without the association), runs the
  installed program's `--check-engine` and `--smoke-test`, and uninstalls it.

## Alternatives

- **WiX (MSI):** better for Group Policy deployment, but more work to keep and
  a .NET SDK on the build machine; reconsider if a school's IT asks for MSI.
- **Zip only:** no Start menu entry, file association or uninstaller.

## Consequences

- Release builds need Inno Setup on the build machine (`winget install
  JRSoftware.InnoSetup`, user scope).
- The installer is unsigned like the zip, so Windows SmartScreen may warn on
  first run until code signing is arranged.

# Disposable CubeIDE startup and shutdown

The preserved `joint-ide-05` timeout contains nested modal event loops from
MCUAnalyticsStartup and WindowsDefenderConfigurator around an Eclipse
PerspectiveManager NullPointerException. This supports a startup-dialog/close
interaction hypothesis, not proof that every shutdown failure has one cause.

Inspecting the installed bundle bytecode showed the supported
`-disableAnalytics` application argument and the preference
`org.eclipse.ui/windows.defender.startup.check.skip`. The experiment
now writes that preference to its newly created configuration's `.settings`
directory and passes the analytics flag to its own IDE process. The preference
skips the suggestion to exclude directories from scanning; it does not disable
Windows protection or create any exclusion. The installation and earlier
workspaces/configurations remain untouched. No analytics agreement is accepted.

Before launching the debug sequence, the owned startup extension verifies both
settings. On completion it records the actual return value from workbench close.
The driver requires `accepted=true`, process exit zero and the existing session
markers. Existing 150-second overall and simulation/numerical deadlines remain
unchanged. Keep every failed attempt. Require three fresh joint-profile runs and
a default guarded lifecycle regression with identical nine stops/circuit values.
This validates disposable automated sessions, not unrestricted interactive IDE
shutdown behavior or a patch to Eclipse's internal PerspectiveManager.

## Preference namespace correction

The initial `shutdown-joint-01` candidate wrote and verified the wrong node,
`org.eclipse.ui.workbench`. Its startup policy marker therefore did not prove
that WindowsDefenderConfigurator saw the setting. The user still saw the dialog
and selected Keep STM32CubeIDE being scanned, then Proceed. The process finally
exited with code zero, but this is assisted evidence, not an unattended pass.

The resulting configuration contains `org.eclipse.ui.prefs`. Inspection of the
installed WorkbenchPlugin static initializer independently confirms that
PI_WORKBENCH is `org.eclipse.ui`. Both the writer and runtime check now use that
node. Validate with three entirely new configurations and a lifecycle regression;
do not reuse the manually confirmed configuration. No Defender exclusion or
system protection setting is changed.

## Verified result

Three new joint sessions (`shutdown-joint-02` through `04`) and one default
guarded lifecycle session (`shutdown-lifecycle-01`) passed without manual dialog
confirmation. All four recorded workbench close acceptance, exited zero, and
removed owned backend/relay listeners. The joint sessions preserved ADC code
3541; the lifecycle's nine stops and circuit records exactly matched the earlier
baseline and reset/reconnect passed. All configurations were created independently.

The corrected startup settings removed the Defender-dialog stack from these
logs. PerspectiveManager's NullPointerException still appears, so the evidence
supports unattended closure of these experiment sessions, not a fix to that
internal Eclipse error. Keep it as a separate issue.
[Evidence and inspected bundle hashes](../../../docs/experiments/evidence/E-05-ide-shutdown-summary.json)
retain the user-assisted candidate and the remaining limitation.

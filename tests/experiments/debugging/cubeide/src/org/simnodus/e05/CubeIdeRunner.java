package org.simnodus.e05;

import java.io.IOException;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.time.Instant;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import org.eclipse.ui.console.ConsolePlugin;
import org.eclipse.ui.console.IConsole;
import org.eclipse.ui.console.MessageConsole;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.eclipse.cdt.dsf.concurrent.DataRequestMonitor;
import org.eclipse.cdt.dsf.concurrent.Query;
import org.eclipse.cdt.dsf.debug.service.IRunControl;
import org.eclipse.cdt.dsf.datamodel.DMContexts;
import org.eclipse.cdt.dsf.debug.service.command.ICommand;
import org.eclipse.cdt.dsf.debug.service.command.ICommandResult;
import org.eclipse.cdt.dsf.gdb.launching.GdbLaunch;
import org.eclipse.cdt.dsf.mi.service.IMICommandControl;
import org.eclipse.cdt.dsf.mi.service.IMIContainerDMContext;
import org.eclipse.cdt.dsf.mi.service.IMIExecutionDMContext;
import org.eclipse.cdt.dsf.mi.service.IMIProcesses;
import org.eclipse.cdt.dsf.mi.service.command.CommandFactory;
import org.eclipse.cdt.dsf.mi.service.command.commands.CLICommand;
import org.eclipse.cdt.dsf.mi.service.command.output.MIDataEvaluateExpressionInfo;
import org.eclipse.cdt.dsf.mi.service.command.output.MIDataReadMemoryBytesInfo;
import org.eclipse.cdt.dsf.mi.service.command.output.MIFrame;
import org.eclipse.cdt.dsf.mi.service.command.output.MIInfo;
import org.eclipse.cdt.dsf.mi.service.command.output.MIStackListFramesInfo;
import org.eclipse.cdt.dsf.service.DsfServicesTracker;
import org.eclipse.cdt.dsf.service.DsfSession;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Platform;
import org.eclipse.debug.core.DebugPlugin;
import org.eclipse.debug.core.ILaunchConfigurationType;
import org.eclipse.debug.core.ILaunchConfigurationWorkingCopy;
import org.eclipse.debug.core.ILaunchManager;
import org.eclipse.debug.core.model.IProcess;
import org.eclipse.debug.core.model.MemoryByte;
import org.eclipse.equinox.app.IApplication;
import org.eclipse.equinox.app.IApplicationContext;
import org.osgi.framework.FrameworkUtil;

public final class CubeIdeRunner implements IApplication {
    private static final String LAUNCH_TYPE =
        "org.eclipse.cdt.debug.gdbjtag.launchConfigurationType";
    private static final Pattern TIME = Pattern.compile(
        "Elapsed Virtual Time: (\\d+):(\\d+):(\\d+)\\.(\\d{9})");

    private static void terminateOwnedDebugger(Map<String, String> arguments, PrintStream stream) {
        Path expectedGdb = Path.of(required(arguments, "--gdb")).toAbsolutePath().normalize();
        var candidates = ProcessHandle.current().descendants()
            .filter(handle -> handle.info().command().map(command ->
                Path.of(command).toAbsolutePath().normalize().equals(expectedGdb)).orElse(false))
            .toList();
        require(candidates.size() == 1, "Owned session GDB process is ambiguous");
        ProcessHandle debugger = candidates.get(0);
        stream.println("REQUEST_JOINT_DISCONNECT pid=" + debugger.pid());
        require(debugger.destroyForcibly(), "Could not terminate owned GDB process");
    }

    @Override
    public Object start(IApplicationContext context) {
        String[] rawArguments = (String[]) context.getArguments()
            .get(IApplicationContext.APPLICATION_ARGS);
        Map<String, String> arguments = parseArguments(rawArguments);
        Path output = Path.of(required(arguments, "--simnodus-output"));
        try (PrintStream stream = new PrintStream(
                Files.newOutputStream(output), true, "UTF-8")) {
            run(arguments, stream);
            return IApplication.EXIT_OK;
        } catch (Exception error) {
            throw new IllegalStateException("CubeIDE experiment runner failed", error);
        }
    }

    static void run(Map<String, String> arguments, PrintStream stream) throws Exception {
        if (arguments.getOrDefault("--joint-lifecycle", "false").equals("true")) {
            Map<String, String> first = new HashMap<>(arguments);
            first.put("--joint-lifecycle", "false");
            run(first, stream);
            stream.println("REQUEST_JOINT_RESET");
            Path resetProperties = Path.of(required(arguments, "--reset-properties"));
            waitForFile(resetProperties, Duration.ofSeconds(45));
            Properties properties = new Properties();
            try (var reader = Files.newBufferedReader(resetProperties, StandardCharsets.UTF_8)) {
                properties.load(reader);
            }
            Map<String, String> second = new HashMap<>();
            for (String key : properties.stringPropertyNames()) {
                second.put("--" + key, properties.getProperty(key));
            }
            second.put("--joint-lifecycle", "false");
            second.put("--profile", "reset");
            run(second, stream);
            stream.println("JOINT_SECOND_SEQUENCE");
            second.put("--profile", "joint");
            run(second, stream);
            stream.println("SIMNODUS_JOINT_LIFECYCLE_OK");
            return;
        }
        stream.println("SIMNODUS_CUBEIDE_RUNNER_START");
        stream.println("product=" + Platform.getProduct().getId());
        stream.println("productName=" + Platform.getProduct().getName());
        stream.println("location=" + Platform.getInstallLocation().getURL());
        stream.println("buildId=" + System.getProperty("eclipse.buildId"));
        for (String bundleName : new String[] {"org.eclipse.cdt.dsf.gdb",
                "org.eclipse.cdt.debug.gdbjtag.core", "org.eclipse.debug.core"}) {
            stream.println("bundle=" + bundleName + " version="
                + Platform.getBundle(bundleName).getVersion());
        }

        ILaunchManager manager = DebugPlugin.getDefault().getLaunchManager();
        ILaunchConfigurationType type = manager.getLaunchConfigurationType(LAUNCH_TYPE);
        require(type != null, "Generic GDB hardware launch type is unavailable");
        ILaunchConfigurationWorkingCopy configuration = configure(type, arguments);
        GdbLaunch launch = (GdbLaunch) configuration.doSave().launch(
            ILaunchManager.DEBUG_MODE, new NullProgressMonitor());
        stream.println("launchClass=" + launch.getClass().getName());

        DsfSession session = launch.getSession();
        // Extension instantiation alone does not start this experiment bundle.
        FrameworkUtil.getBundle(CubeIdeRunner.class).start();
        DsfServicesTracker tracker = new DsfServicesTracker(
            FrameworkUtil.getBundle(CubeIdeRunner.class).getBundleContext(), session.getId());
        try {
            IMICommandControl control = requireService(
                tracker, IMICommandControl.class, Duration.ofSeconds(10));
            IMIProcesses processes = requireService(
                tracker, IMIProcesses.class, Duration.ofSeconds(10));
            IRunControl runControl = requireService(
                tracker, IRunControl.class, Duration.ofSeconds(10));
            CommandFactory factory = control.getCommandFactory();
            var processContext = processes.createProcessContext(control.getContext(), "1");
            var threadContext = processes.createThreadContext(processContext, "1");
            IMIContainerDMContext container = processes.createContainerContext(
                processContext, "i1");
            IMIExecutionDMContext execution = processes.createExecutionContext(
                container, threadContext, "1");
            waitForSuspended(runControl, session, execution, Duration.ofSeconds(10));
            long attached = virtualTime(session, control, execution);
            require(attached == 0, "IDE attach advanced virtual time");
            stream.println("attachTimeNs=" + attached);
            for (IProcess process : launch.getProcesses()) {
                stream.println("processLabel=" + process.getLabel());
            }

            String profile = arguments.getOrDefault("--profile", "normal");
            if (profile.equals("joint")) {
                Path directory = Path.of(required(arguments, "--cooperative-directory"));
                insertBreakpoint(session, control, execution, "gpio_change_marker");
                insertBreakpoint(session, control, execution, "*0x08000102");
                insertBreakpoint(session, control, execution, "adc_read_marker");
                insertBreakpoint(session, control, execution, "*0x080001b8");
                boolean jointSteps = Boolean.parseBoolean(arguments.getOrDefault("--joint-steps", "false"));
                int pauseIndex = jointSteps ? 7 : 2;
                int commitIndex = jointSteps ? 9 : 4;
                if (jointSteps) { insertBreakpoint(session, control, execution, "step_target"); }
                boolean race = Boolean.parseBoolean(arguments.getOrDefault("--joint-race", "false"));
                boolean collisionBreakpoint = false;
                boolean breakpointFirst = Boolean.parseBoolean(arguments.getOrDefault("--joint-breakpoint-first", "false"));
                long[] expectedTimes = jointSteps ? new long[] {2010875,2011000,2011375,2012125,2012250,2012500,2012875,-1,4010750,4027000}
                    : new long[] {2010875,2011375,-1,4010750,4027000};
                long[] expectedPcs = jointSteps ? new long[] {0x080000f8L,0x080000faL,0x08000102L,0x08000120L,0x08000122L,0x08000126L,0x0800012aL,0,0x08000134L,0x080001b8L}
                    : new long[] {0x080000f8L,0x08000102L,0,0x08000134L,0x080001b8L};
                int previousSourceLine = -1;
                for (int index = 0; index < expectedTimes.length; index++) {
                    stream.println("REQUEST_JOINT_START_" + index);
                    waitForFile(directory.resolve("joint-ready-" + index), Duration.ofSeconds(15));
                    if (jointSteps && index == 1) {
                        submit(session, control, factory.createMIExecStepInstruction(execution));
                    } else if (jointSteps && index >= 4 && index <= 6) {
                        submit(session, control, factory.createMIExecNext(execution));
                    } else {
                        submit(session, control, factory.createMIExecContinue(execution));
                    }
                    long requested = 0;
                    if (index == pauseIndex) {
                        if (breakpointFirst) {
                            waitForPc(session, control, factory, runControl, execution, 0x08000134L, Duration.ofSeconds(10));
                            require(stopReason(session, execution).equals("BREAKPOINT")
                                && virtualTime(session, control, execution) == 4010750,
                                "Breakpoint-first requires the real ADC breakpoint");
                            requested = System.nanoTime();
                            stream.println("REQUEST_JOINT_BREAKPOINT_PAUSE");
                        } else {
                            waitForFile(directory.resolve("joint-active"), Duration.ofSeconds(2));
                            requested = System.nanoTime();
                            if (race) { stream.println("REQUEST_JOINT_RACE_INTERRUPT"); }
                            String jointFault = arguments.getOrDefault("--joint-fault", "none");
                            boolean analogFault = !jointFault.equals("none");
                            if (jointFault.equals("backend") && !race) {
                                stream.println("REQUEST_JOINT_BACKEND_LOSS");
                            } else if (jointFault.equals("disconnect") && !race) {
                                terminateOwnedDebugger(arguments, stream);
                            } else {
                                try {
                                    submit(session, control, factory.createMIExecInterrupt(execution));
                                } catch (Exception error) {
                                    if (!analogFault) { throw error; }
                                    stream.println("JOINT_INTERRUPT_ENDED " + error.getClass().getSimpleName());
                                }
                            }
                            if (analogFault) {
                                if (race) {
                                    waitForFile(directory.resolve(jointFault.equals("backend") ? "joint-race-backend-ready" : jointFault.equals("disconnect") ? "joint-race-disconnect-ready" : "joint-race-candidate"), Duration.ofSeconds(2));
                                    waitForPc(session, control, factory, runControl, execution, 0x08000134L, Duration.ofSeconds(2));
                                    require(stopReason(session, execution).equals("BREAKPOINT")
                                        && virtualTime(session, control, execution) == 4010750,
                                        "Race fault requires the actual ADC breakpoint");
                                    stream.println("JOINT_RACE_BREAKPOINT_CONFIRMED");
                                    if (jointFault.equals("backend")) {
                                        stream.println("REQUEST_JOINT_BACKEND_LOSS");
                                    } else if (jointFault.equals("disconnect")) {
                                        terminateOwnedDebugger(arguments, stream);
                                    }
                                }
                                long failureSeconds = jointFault.equals("timeout") ? 5 : 2;
                                waitForFile(directory.resolve("joint-failed"), Duration.ofSeconds(failureSeconds));
                                require(System.nanoTime() - requested <= failureSeconds * 1_000_000_000L,
                                    "Joint failure diagnosis exceeded the declared deadline");
                                require(!Files.exists(directory.resolve("joint-stopped-" + pauseIndex))
                                    && !Files.exists(directory.resolve("joint-notified")), "Failed joint pause was published");
                                publishDiagnostic(stream, Files.readString(directory.resolve("joint-failed")));
                                stream.println("SIMNODUS_JOINT_FAILURE_OK");
                                return;
                            }
                        }
                    }
                    if (index == pauseIndex) {
                        waitForSuspended(runControl, session, execution, Duration.ofSeconds(2));
                        if (race) {
                            long arbitrationDeadline = System.nanoTime() + 2_000_000_000L;
                            while (!Files.exists(directory.resolve("joint-race-candidate"))
                                && !Files.exists(directory.resolve("joint-stopped-" + pauseIndex)) && System.nanoTime() < arbitrationDeadline) {
                                Thread.sleep(10);
                            }
                            require(Files.exists(directory.resolve("joint-race-candidate"))
                                || Files.exists(directory.resolve("joint-stopped-" + pauseIndex)), "Race arbitration endpoint missing");
                        }
                        if (race && Files.exists(directory.resolve("joint-race-candidate"))) {
                            waitForPc(session, control, factory, runControl, execution, 0x08000134L, Duration.ofSeconds(2));
                            require(stopReason(session, execution).equals("BREAKPOINT")
                                && virtualTime(session, control, execution) == 4010750
                                && evaluateLong(session, control, factory, execution, "$pc") == 0x08000134L,
                                "Race stopped at an unexpected breakpoint");
                            collisionBreakpoint = true;
                            stream.println("JOINT_RACE_BREAKPOINT_CONFIRMED");
                        }
                    } else {
                        waitForPc(session, control, factory, runControl, execution, expectedPcs[index], Duration.ofSeconds(10));
                    }
                    if (index != pauseIndex) {
                        require(virtualTime(session, control, execution) == expectedTimes[index],
                            "Joint fixture breakpoint time changed");
                        stream.println("REQUEST_JOINT_STOP_" + index);
                    }
                    waitForFile(directory.resolve("joint-stopped-" + index), Duration.ofSeconds(2));
                    long expected = Long.parseLong(Files.readString(directory.resolve("joint-stopped-" + index)));
                    require(virtualTime(session, control, execution) == expected, "Joint IDE clock disagrees");
                    String reason = stopReason(session, execution);
                    if (index == pauseIndex) {
                        require(System.nanoTime() - requested <= 2_000_000_000L, "Joint interrupt exceeded two seconds");
                        if (breakpointFirst || collisionBreakpoint) {
                            require(reason.equals("BREAKPOINT") && expected == 4010750,
                                "Breakpoint-first changed the actual stop reason or endpoint");
                            require(!Files.exists(directory.resolve("joint-notified")), "Breakpoint-first fabricated a signal");
                        } else {
                            require(reason.equals("SIGNAL"), "Joint pause omitted real signal stop");
                            require(expected > (jointSteps ? 2012875 : 2011375) && expected < 4010750, "Joint interrupt escaped GPIO/ADC interval");
                        }
                        publishDiagnostic(stream, "CPU and persistent RC circuit paused at " + expected + " ns.");
                    }
                    if (jointSteps && index != pauseIndex) {
                        boolean stepped = index == 1 || (index >= 4 && index <= 6);
                        require(reason.equals(stepped ? "STEP" : "BREAKPOINT"), "Persistent step stop reason differs");
                    }
                    if (jointSteps && index >= 3 && index <= 6) {
                        MIFrame[] frames = submit(session, control, factory.createMIStackListFrames(execution, 0, 0)).getMIFrames();
                        require(frames.length == 1 && frames[0].getFunction().equals("step_target")
                            && frames[0].getFile().endsWith("firmware.c") && frames[0].getLine() > previousSourceLine,
                            "Persistent next did not advance source within step_target");
                        previousSourceLine = frames[0].getLine();
                        stream.println("JOINT_STEP_SOURCE index=" + index + " function=" + frames[0].getFunction()
                            + " line=" + previousSourceLine);
                    }
                    byte[] mailbox = readMemory(session, control, factory, execution, "0x20000000", 32);
                    long pc = evaluateLong(session, control, factory, execution, "$pc");
                    String[] registers = jointSteps ? new String[] {
                        evaluate(session, control, factory, execution, "$r0"),
                        evaluate(session, control, factory, execution, "$sp"),
                        evaluate(session, control, factory, execution, "$lr")} : null;
                    Thread.sleep(100);
                    require(virtualTime(session, control, execution) == expected
                        && hex(readMemory(session, control, factory, execution, "0x20000000", 32)).equals(hex(mailbox))
                        && evaluateLong(session, control, factory, execution, "$pc") == pc,
                        "Joint IDE inspection changed state");
                    if (jointSteps) {
                        require(evaluate(session, control, factory, execution, "$r0").equals(registers[0])
                            && evaluate(session, control, factory, execution, "$sp").equals(registers[1])
                            && evaluate(session, control, factory, execution, "$lr").equals(registers[2])
                            && virtualTime(session, control, execution) == expected,
                            "Persistent register inspection changed state");
                        stream.println("JOINT_REGISTERS index=" + index + " r0=" + registers[0]
                            + " sp=" + registers[1] + " lr=" + registers[2] + " stable=true");
                    }
                    if (index == commitIndex) {
                        require(hex(mailbox).equals("35304e53040000000100000001000000d50d0000010000000000000000000000"),
                            "Joint firmware ADC result differs");
                    }
                    stream.println("JOINT_INSPECTED_" + index + " timeNs=" + expected + " pc=" + pc
                        + " reason=" + reason + " mailbox=" + hex(mailbox));
                    waitForFile(directory.resolve("joint-inspected-" + index), Duration.ofSeconds(2));
                    if (index == pauseIndex && (breakpointFirst || collisionBreakpoint)) { index++; }
                }
                stream.println("REQUEST_RENODE_STOP");
                waitForFile(Path.of(required(arguments, "--renode-stopped")), Duration.ofSeconds(20));
                stream.println("SIMNODUS_JOINT_IDE_OK");
                return;
            }
            if (profile.equals("cooperative")) {
                Path directory = Path.of(required(arguments, "--cooperative-directory"));
                stream.println("REQUEST_COOP_START");
                waitForFile(directory.resolve("coop-ready"), Duration.ofSeconds(15));
                submit(session, control, factory.createMIExecContinue(execution));
                Thread.sleep(10);
                long requested = System.nanoTime();
                submit(session, control, factory.createMIExecInterrupt(execution));
                waitForSuspended(runControl, session, execution, Duration.ofSeconds(2));
                waitForFile(directory.resolve("coop-paused"), Duration.ofSeconds(2));
                long elapsed = System.nanoTime() - requested;
                require(elapsed <= 2_000_000_000L, "IDE cooperative pause exceeded two seconds");
                long expected = Long.parseLong(Files.readString(directory.resolve("coop-paused")));
                long observed = virtualTime(session, control, execution);
                require(observed == expected, "IDE pause time differs from host acknowledgement");
                byte[] mailbox = readMemory(session, control, factory, execution, "0x20000000", 32);
                long pc = evaluateLong(session, control, factory, execution, "$pc");
                String reason = stopReason(session, execution);
                require(reason.equals("SIGNAL"), "Cooperative pause was not reported as a signal stop");
                Thread.sleep(100);
                require(virtualTime(session, control, execution) == expected
                    && hex(readMemory(session, control, factory, execution, "0x20000000", 32)).equals(hex(mailbox))
                    && evaluateLong(session, control, factory, execution, "$pc") == pc,
                    "IDE inspection changed a paused state");
                stream.println("COOPERATIVE_PAUSED timeNs=" + observed + " pc=" + pc + " reason=" + reason
                    + " wallNs=" + elapsed + " mailbox=" + hex(mailbox));
                publishDiagnostic(stream, "CPU backend paused at " + observed + " ns. Joint analog pause is not validated.");
                stream.println("REQUEST_COOP_RESUME");
                waitForFile(directory.resolve("coop-resume-ready"), Duration.ofSeconds(10));
                submit(session, control, factory.createMIExecContinue(execution));
                waitForFile(directory.resolve("coop-complete"), Duration.ofSeconds(10));
                long resumed = Long.parseLong(Files.readString(directory.resolve("coop-complete")));
                require(resumed == expected + 5_000_000L, "IDE resume did not honor the new interval");
                stream.println("COOPERATIVE_RESUMED timeNs=" + resumed);
                stream.println("REQUEST_RENODE_STOP");
                waitForFile(Path.of(required(arguments, "--renode-stopped")), Duration.ofSeconds(20));
                stream.println("SIMNODUS_COOPERATIVE_IDE_OK");
                return;
            }
            if (profile.equals("reattach")) {
                stream.println("RECONNECTED timeNs=" + attached);
                return;
            }
            if (profile.equals("reset")) {
                byte[] initial = readMemory(session, control, factory, execution, "0x20000000", 32);
                require(hex(initial).equals("00".repeat(32)), "Reset mailbox was not empty");
                stream.println("RESET timeNs=0 mailbox=" + hex(initial));
                stream.println("RESET_CIRCUIT " + oneLine(runNative(arguments, "circuit", "0", "none", "none")));
                submit(session, control, new CLICommand<>(execution, "detach"));
                stream.println("DISCONNECTED");
                waitForFile(Path.of(required(arguments, "--disconnect-checked")), Duration.ofSeconds(20));
                // DSF terminates the launch after detach. Reconnect with a fresh launch.
                waitForTermination(launch, Duration.ofSeconds(10));
                Map<String, String> reconnectArguments = new HashMap<>(arguments);
                reconnectArguments.put("--profile", "reattach");
                run(reconnectArguments, stream);
                stream.println("DEBUGGER_RESET_POLICY debugger-only reset is unsupported");
                return;
            }
            if (profile.equals("pause")) {
                submit(session, control, factory.createMIExecContinue(execution));
                Process interval = startNative(arguments, "control", "run", "100000");
                try {
                    Thread.sleep(10);
                    long started = System.nanoTime();
                    submit(session, control, factory.createMIExecInterrupt(execution));
                    waitForSuspended(runControl, session, execution, Duration.ofSeconds(2));
                    long elapsed = System.nanoTime() - started;
                    require(elapsed <= 2_000_000_000L, "Interrupt exceeded two seconds");
                    long observed = virtualTime(session, control, execution);
                    require(interval.waitFor(35, TimeUnit.SECONDS), "Interrupted grant timed out");
                    String intervalOutput = new String(interval.getInputStream().readAllBytes(), StandardCharsets.UTF_8);
                    require(interval.exitValue() == 0, "Interrupted grant failed");
                    long effective = virtualTime(session, control, execution);
                    require(observed >= 0 && observed <= 100_000_000L && effective == 100_000_000L,
                        "Unexpected interrupted global time");
                    readMemory(session, control, factory, execution, "0x20000000", 32);
                    require(virtualTime(session, control, execution) == effective,
                        "Inspection after grant stabilization advanced time");
                    stream.println("PAUSE observedNs=" + observed + " effectiveNs=" + effective
                        + " wallNs=" + elapsed + " committed=false supported=false grant=" + oneLine(intervalOutput));
                    return;
                } finally {
                    if (interval.isAlive()) { interval.destroyForcibly(); }
                }
            }
            require(profile.equals("normal") || profile.equals("lifecycle"), "Unknown action profile");

            insertBreakpoint(session, control, execution, "gpio_change_marker");
            insertBreakpoint(session, control, execution, "*0x08000102");
            insertBreakpoint(session, control, execution, "step_target");
            insertBreakpoint(session, control, execution, "adc_read_marker");
            insertBreakpoint(session, control, execution, "*0x080001b8");
            runNative(arguments, "control", "adc", "0", "1650000");
            if (Boolean.parseBoolean(arguments.getOrDefault("--guarded", "false"))) {
                stream.println("REQUEST_HOST_GRANT");
                waitForFile(Path.of(required(arguments, "--guard-ready")), Duration.ofSeconds(10));
            }

            submit(session, control, factory.createMIExecContinue(execution));
            Process grant = startNative(arguments, "control", "run", "5000");
            try {
                waitForPc(session, control, factory, runControl, execution,
                    0x080000f8L, Duration.ofSeconds(20));
                recordStop(arguments, stream, session, control, factory, execution,
                    "gpio", 0x080000f8L, null);

                String fault = arguments.getOrDefault("--fault", "none");
                if (!fault.equals("none")) {
                    require(grant.isAlive(), "Fault requires a pending host grant");
                    if (fault.equals("backend")) {
                        stream.println("REQUEST_BACKEND_FAILURE");
                    } else if (fault.equals("timeout")) {
                        submit(session, control, factory.createMIExecContinue(execution));
                        waitForPc(session, control, factory, runControl, execution,
                            0x08000102L, Duration.ofSeconds(10));
                        long before = virtualTime(session, control, execution);
                        long started = System.nanoTime();
                        boolean expired = false;
                        try {
                            waitForPc(session, control, factory, runControl, execution,
                                0x0800fffcL, Duration.ofSeconds(2));
                        } catch (IllegalStateException expected) {
                            require(expected.getMessage().equals("CubeIDE did not stop at 0x800fffc"),
                                "Unexpected stop failure: " + expected);
                            expired = true;
                        }
                        long elapsed = System.nanoTime() - started;
                        require(expired && elapsed >= 2_000_000_000L && elapsed <= 5_000_000_000L,
                            "Unreachable stop did not expire within the declared deadline window");
                        require(evaluateLong(session, control, factory, execution, "$pc") == 0x08000102L
                            && virtualTime(session, control, execution) == before && grant.isAlive(),
                            "Timeout changed the held stop or completed the grant");
                        stream.println("STOP_TIMEOUT wallNs=" + elapsed
                            + " expectedPc=0x800fffc observedPc=0x8000102 timeNs=" + before);
                        stream.println("REQUEST_STOP_TIMEOUT");
                    } else {
                        throw new IllegalArgumentException("Unknown fault");
                    }
                    Path diagnosis = Path.of(required(arguments, "--guard-rejection"));
                    waitForFile(diagnosis, Duration.ofSeconds(20));
                    publishDiagnostic(stream, Files.readString(diagnosis, StandardCharsets.UTF_8));
                    require(grant.waitFor(10, TimeUnit.SECONDS), "Failed grant did not finish");
                    require(grant.exitValue() != 0, "Failed grant unexpectedly committed");
                    stream.println("FAILED_GRANT_EXIT=" + grant.exitValue());
                    return;
                }

                String rejectedAction = arguments.getOrDefault("--reject-action", "none");
                if (!rejectedAction.equals("none")) {
                    stream.println("REQUEST_UNSUPPORTED_ACTION " + rejectedAction);
                    try {
                        if (rejectedAction.equals("reset")) {
                            submit(session, control, new CLICommand<>(execution, "monitor machine Reset"));
                        } else if (rejectedAction.equals("detach")) {
                            submit(session, control, new CLICommand<>(execution, "detach"));
                        } else if (rejectedAction.equals("interrupt")) {
                            submit(session, control, factory.createMIExecInterrupt(execution));
                        } else {
                            throw new IllegalArgumentException("Unknown rejection test");
                        }
                    } catch (Exception expected) {
                        stream.println("REJECTED_COMMAND_RESPONSE " + oneLine(expected.toString()));
                    }
                    Path diagnosis = Path.of(required(arguments, "--guard-rejection"));
                    waitForFile(diagnosis, Duration.ofSeconds(10));
                    String message = Files.readString(diagnosis, StandardCharsets.UTF_8);
                    publishDiagnostic(stream, message);
                    require(grant.waitFor(10, TimeUnit.SECONDS), "Rejected grant did not finish");
                    require(grant.exitValue() != 0, "Rejected grant unexpectedly committed");
                    stream.println("REJECTED_GRANT_EXIT=" + grant.exitValue());
                    return;
                }

                submit(session, control, factory.createMIExecStepInstruction(execution));
                waitForPc(session, control, factory, runControl, execution,
                    0x080000faL, Duration.ofSeconds(10));
                recordStop(arguments, stream, session, control, factory, execution,
                    "instruction", 0x080000faL, null);

                submit(session, control, factory.createMIExecContinue(execution));
                waitForPc(session, control, factory, runControl, execution,
                    0x08000102L, Duration.ofSeconds(10));
                long rise = virtualTime(session, control, execution);
                byte[] odr = readMemory(session, control, factory, execution, "0x4001080c", 4);
                require((word(odr, 0) & 1) != 0, "GPIO was not high after the write");
                recordStop(arguments, stream, session, control, factory, execution,
                    "gpioEdge", 0x08000102L, rise);

                submit(session, control, factory.createMIExecContinue(execution));
                waitForPc(session, control, factory, runControl, execution,
                    0x08000120L, Duration.ofSeconds(10));
                recordStop(arguments, stream, session, control, factory, execution,
                    "stepTarget", 0x08000120L, rise);

                long[] nextPcs = {0x08000122L, 0x08000126L, 0x0800012aL};
                for (int index = 0; index < nextPcs.length; index++) {
                    submit(session, control, factory.createMIExecNext(execution));
                    waitForPc(session, control, factory, runControl, execution,
                        nextPcs[index], Duration.ofSeconds(10));
                    recordStop(arguments, stream, session, control, factory, execution,
                        "next" + (index + 1), nextPcs[index], rise);
                }

                submit(session, control, factory.createMIExecContinue(execution));
                waitForPc(session, control, factory, runControl, execution,
                    0x08000134L, Duration.ofSeconds(10));
                recordStop(arguments, stream, session, control, factory, execution,
                    "adc", 0x08000134L, rise);

                submit(session, control, factory.createMIExecContinue(execution));
                waitForPc(session, control, factory, runControl, execution,
                    0x080001b8L, Duration.ofSeconds(10));
                long committed = recordStop(arguments, stream, session, control, factory,
                    execution, "adcCommitted", 0x080001b8L, rise);
                byte[] mailbox = readMemory(
                    session, control, factory, execution, "0x20000000", 32);
                require(word(mailbox, 0) == 0x534e3035L, "Firmware magic changed");
                require(word(mailbox, 16) == 2048, "IDE path did not observe ADC result");
                require(word(mailbox, 4) == 4 && word(mailbox, 8) == 1
                    && word(mailbox, 12) == 1 && word(mailbox, 20) == 1
                    && word(mailbox, 24) == 0, "Unexpected firmware observations");
                stream.println("committedTimeNs=" + committed);
                stream.println("REQUEST_RENODE_STOP");
                waitForFile(Path.of(required(arguments, "--renode-stopped")),
                    Duration.ofSeconds(20));
                String grantOutput = new String(
                    grant.getInputStream().readAllBytes(), StandardCharsets.UTF_8).trim();
                grant.waitFor(10, TimeUnit.SECONDS);
                stream.println("grantExit=" + grant.exitValue());
                stream.println("grantOutput=" + oneLine(grantOutput));
                require(grant.exitValue() != 0,
                    "Pending host grant committed after coordinated backend stop");
            } finally {
                if (grant.isAlive()) {
                    grant.destroyForcibly();
                }
            }
        } finally {
            tracker.dispose();
            for (IProcess process : launch.getProcesses()) {
                if (process.canTerminate()) {
                    process.terminate();
                }
            }
            waitForTermination(launch, Duration.ofSeconds(10));
            stream.println("SIMNODUS_CUBEIDE_SESSION_CLOSED");
        }
        stream.println("SIMNODUS_CUBEIDE_RUNNER_OK");
        if (arguments.getOrDefault("--profile", "normal").equals("lifecycle")) {
            stream.println("REQUEST_SESSION_RESET");
            Path resetProperties = Path.of(required(arguments, "--reset-properties"));
            waitForFile(resetProperties, Duration.ofSeconds(45));
            Properties reset = new Properties();
            try (var reader = Files.newBufferedReader(resetProperties, StandardCharsets.UTF_8)) {
                reset.load(reader);
            }
            Map<String, String> resetArguments = new HashMap<>();
            for (String key : reset.stringPropertyNames()) {
                resetArguments.put("--" + key, reset.getProperty(key));
            }
            run(resetArguments, stream);
            stream.println("SIMNODUS_LIFECYCLE_OK");
        }
    }

    private static void publishDiagnostic(PrintStream stream, String message) throws Exception {
        MessageConsole console = new MessageConsole("SimNodus experiment", null);
        ConsolePlugin.getDefault().getConsoleManager().addConsoles(new IConsole[] {console});
        try (var consoleStream = console.newMessageStream()) {
            consoleStream.println(message);
        }
        console.activate();
        Instant consoleDeadline = Instant.now().plusSeconds(5);
        while (!console.getDocument().get().contains(message) && Instant.now().isBefore(consoleDeadline)) {
            Thread.sleep(20);
        }
        require(console.getDocument().get().contains(message), "IDE console omitted the diagnostic");
        stream.println("IDE_DIAGNOSTIC " + oneLine(message));
    }

    private static ILaunchConfigurationWorkingCopy configure(
            ILaunchConfigurationType type, Map<String, String> arguments) throws Exception {
        String elf = required(arguments, "--elf");
        String gdb = required(arguments, "--gdb");
        String port = required(arguments, "--port");
        ILaunchConfigurationWorkingCopy configuration =
            type.newInstance(null, "SimNodus E-05 CubeIDE session");
        configuration.setAttribute("org.eclipse.cdt.launch.PROJECT_ATTR", "");
        configuration.setAttribute("org.eclipse.cdt.launch.PROGRAM_NAME", elf);
        configuration.setAttribute("org.eclipse.cdt.launch.ATTR_BUILD_BEFORE_LAUNCH_ATTR", 0);
        configuration.setAttribute("org.eclipse.cdt.launch.DEBUGGER_START_MODE", "remote");
        configuration.setAttribute("org.eclipse.cdt.dsf.gdb.DEBUG_NAME", gdb);
        configuration.setAttribute("org.eclipse.cdt.dsf.gdb.GDB_INIT", "");
        configuration.setAttribute("org.eclipse.cdt.dsf.gdb.NON_STOP", false);
        configuration.setAttribute("org.eclipse.cdt.dsf.gdb.REMOTE_TCP", true);
        configuration.setAttribute("org.eclipse.cdt.dsf.gdb.HOST", "127.0.0.1");
        configuration.setAttribute("org.eclipse.cdt.dsf.gdb.PORT", port);
        configuration.setAttribute("org.eclipse.cdt.dsf.gdb.REMOTE_TIMEOUT_ENABLED", true);
        configuration.setAttribute("org.eclipse.cdt.dsf.gdb.REMOTE_TIMEOUT_VALUE", "20");
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.useRemoteTarget", true);
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.ipAddress", "127.0.0.1");
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.portNumber", port);
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.connection",
            "tcp:127.0.0.1:" + port);
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.protocol", "remote");
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.jtagDeviceId",
            "org.eclipse.cdt.debug.gdbjtag.core.jtagdevice.genericDevice");
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.doReset", false);
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.doHalt", false);
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.loadImage", false);
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.loadSymbols", true);
        configuration.setAttribute(
            "org.eclipse.cdt.debug.gdbjtag.core.useProjBinaryForSymbols", false);
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.useFileForSymbols", true);
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.symbolsFileName", elf);
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.setPcRegister", false);
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.setStopAt", false);
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.setResume", false);
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.initCommands", "");
        configuration.setAttribute("org.eclipse.cdt.debug.gdbjtag.core.runCommands", "");
        return configuration;
    }

    private static void insertBreakpoint(DsfSession session, IMICommandControl control,
            IMIExecutionDMContext execution, String location)
            throws Exception {
        MIInfo result = submit(
            session, control, new CLICommand<>(execution, "break " + location));
        require(result.isDone(), "CubeIDE could not insert breakpoint " + location);
    }

    private static long recordStop(Map<String, String> arguments, PrintStream stream,
            DsfSession session, IMICommandControl control, CommandFactory factory,
            IMIExecutionDMContext execution, String label, long expectedPc, Long rise)
            throws Exception {
        long before = virtualTime(session, control, execution);
        require(before >= 0 && before < 5_000_000L, "Stop outside host grant");
        String reason = stopReason(session, execution);
        String expectedReason = label.equals("instruction") || label.startsWith("next") ? "STEP" : "BREAKPOINT";
        require(reason.equals(expectedReason), "Unexpected stop reason: " + reason);
        long pc = evaluateLong(session, control, factory, execution, "$pc");
        String r0 = evaluate(session, control, factory, execution, "$r0");
        String sp = evaluate(session, control, factory, execution, "$sp");
        String lr = evaluate(session, control, factory, execution, "$lr");
        MIStackListFramesInfo stack = submit(
            session, control, factory.createMIStackListFrames(execution, 0, 0));
        MIFrame[] frames = stack.getMIFrames();
        require(frames.length == 1, "CubeIDE did not return the top source frame");
        byte[] mailbox = readMemory(
            session, control, factory, execution, "0x20000000", 32);
        long after = virtualTime(session, control, execution);
        require(before == after, "CubeIDE inspection advanced Renode virtual time");
        require(pc == expectedPc, "CubeIDE stopped at an unexpected program counter");
        MIFrame frame = frames[0];
        stream.println("STOP label=" + label + " reason=" + reason + " timeNs=" + before
            + " pc=0x" + Long.toHexString(pc) + " function=" + oneLine(frame.getFunction())
            + " file=" + oneLine(frame.getFile()) + " line=" + frame.getLine()
            + " r0=" + oneLine(r0) + " sp=" + oneLine(sp) + " lr=" + oneLine(lr)
            + " mailbox=" + hex(mailbox));
        String circuit = runNative(arguments, "circuit", Long.toString(before),
            rise == null ? "none" : rise.toString(), "none");
        stream.println("CIRCUIT label=" + label + " result=" + oneLine(circuit));
        return before;
    }

    private static String stopReason(DsfSession session, IMIExecutionDMContext execution)
            throws Exception {
        DsfServicesTracker tracker = new DsfServicesTracker(
            FrameworkUtil.getBundle(CubeIdeRunner.class).getBundleContext(), session.getId());
        try {
            IRunControl runControl = requireService(tracker, IRunControl.class, Duration.ofSeconds(5));
            Query<IRunControl.IExecutionDMData> query = new Query<>() {
                @Override
                protected void execute(DataRequestMonitor<IRunControl.IExecutionDMData> monitor) {
                    runControl.getExecutionData(DMContexts.getAncestorOfType(
                        execution, IRunControl.IContainerDMContext.class), monitor);
                }
            };
            session.getExecutor().execute(query);
            return query.get(5, TimeUnit.SECONDS).getStateChangeReason().toString();
        } finally {
            tracker.dispose();
        }
    }

    private static void waitForPc(DsfSession session, IMICommandControl control,
            CommandFactory factory, IRunControl runControl, IMIExecutionDMContext execution,
            long expectedPc, Duration timeout) throws Exception {
        Instant deadline = Instant.now().plus(timeout);
        while (Instant.now().isBefore(deadline)) {
            if (isSuspended(runControl, session, execution)) {
                try {
                    if (evaluateLong(session, control, factory, execution, "$pc") == expectedPc) {
                        return;
                    }
                } catch (Exception ignored) {
                }
            }
            Thread.sleep(10);
        }
        throw new IllegalStateException(
            "CubeIDE did not stop at 0x" + Long.toHexString(expectedPc));
    }

    private static void waitForSuspended(IRunControl control, DsfSession session,
            IMIExecutionDMContext execution, Duration timeout) throws Exception {
        Instant deadline = Instant.now().plus(timeout);
        while (Instant.now().isBefore(deadline)) {
            if (isSuspended(control, session, execution)) {
                return;
            }
            Thread.sleep(10);
        }
        throw new IllegalStateException("CubeIDE target did not become suspended");
    }

    private static boolean isSuspended(IRunControl control, DsfSession session,
            IMIExecutionDMContext execution) throws Exception {
        Query<Boolean> query = new Query<>() {
            @Override
            protected void execute(DataRequestMonitor<Boolean> monitor) {
                monitor.setData(control.isSuspended(execution));
                monitor.done();
            }
        };
        session.getExecutor().execute(query);
        return query.get(5, TimeUnit.SECONDS);
    }

    private static long virtualTime(DsfSession session, IMICommandControl control,
            IMIExecutionDMContext execution) throws Exception {
        MIInfo info = submit(session, control,
            new CLICommand<>(execution, "monitor machine ElapsedVirtualTime"));
        Matcher match = TIME.matcher(info.toString());
        require(match.find(), "CubeIDE monitor response omitted virtual time: " + info);
        long hours = Long.parseLong(match.group(1));
        long minutes = Long.parseLong(match.group(2));
        long seconds = Long.parseLong(match.group(3));
        long fraction = Long.parseLong(match.group(4));
        return ((hours * 60 + minutes) * 60 + seconds) * 1_000_000_000L + fraction;
    }

    private static String evaluate(DsfSession session, IMICommandControl control,
            CommandFactory factory, IMIExecutionDMContext execution, String expression)
            throws Exception {
        MIDataEvaluateExpressionInfo result = submit(session, control,
            factory.createMIDataEvaluateExpression(execution, expression));
        require(result.isDone(), "CubeIDE expression failed: " + expression);
        return result.getValue();
    }

    private static long evaluateLong(DsfSession session, IMICommandControl control,
            CommandFactory factory, IMIExecutionDMContext execution, String expression)
            throws Exception {
        String value = evaluate(session, control, factory, execution, expression);
        Matcher match = Pattern.compile("0x([0-9a-fA-F]+)").matcher(value);
        require(match.find(), "CubeIDE expression was not hexadecimal: " + value);
        return Long.parseUnsignedLong(match.group(1), 16);
    }

    private static byte[] readMemory(DsfSession session, IMICommandControl control,
            CommandFactory factory, IMIExecutionDMContext execution, String address, int count)
            throws Exception {
        MIDataReadMemoryBytesInfo result = submit(session, control,
            factory.createMIDataReadMemoryBytes(execution, address, 0, count));
        MemoryByte[] values = result.getMIMemoryBlock();
        require(values.length == count, "CubeIDE memory response length changed");
        byte[] bytes = new byte[count];
        for (int index = 0; index < count; index++) {
            bytes[index] = values[index].getValue();
        }
        return bytes;
    }

    private static <V extends ICommandResult> V submit(DsfSession session,
            IMICommandControl control, ICommand<V> command) throws Exception {
        Query<V> query = new Query<>() {
            @Override
            protected void execute(DataRequestMonitor<V> monitor) {
                control.queueCommand(command, monitor);
            }
        };
        session.getExecutor().execute(query);
        return query.get(20, TimeUnit.SECONDS);
    }

    private static <T> T requireService(DsfServicesTracker tracker, Class<T> type,
            Duration timeout) throws InterruptedException {
        Instant deadline = Instant.now().plus(timeout);
        while (Instant.now().isBefore(deadline)) {
            T service = tracker.getService(type);
            if (service != null) {
                return service;
            }
            Thread.sleep(20);
        }
        throw new IllegalStateException("CubeIDE service unavailable: " + type.getName());
    }

    private static Process startNative(Map<String, String> arguments,
            String tool, String... toolArguments) throws IOException {
        String[] command = new String[toolArguments.length + 2];
        command[0] = required(arguments, "--" + tool);
        if (tool.equals("control")) {
            command[1] = required(arguments, "--control-port");
        } else {
            throw new IllegalArgumentException("Only asynchronous control is supported");
        }
        System.arraycopy(toolArguments, 0, command, 2, toolArguments.length);
        return new ProcessBuilder(command).redirectErrorStream(true).start();
    }

    private static String runNative(Map<String, String> arguments,
            String tool, String... toolArguments) throws Exception {
        String[] prefix;
        if (tool.equals("control")) {
            prefix = new String[] {
                required(arguments, "--control"), required(arguments, "--control-port")};
        } else if (tool.equals("circuit")) {
            prefix = new String[] {required(arguments, "--circuit"),
                required(arguments, "--ngspice-dll"), required(arguments, "--audio"),
                required(arguments, "--initialization")};
        } else {
            throw new IllegalArgumentException("Unknown native tool " + tool);
        }
        String[] command = new String[prefix.length + toolArguments.length];
        System.arraycopy(prefix, 0, command, 0, prefix.length);
        System.arraycopy(toolArguments, 0, command, prefix.length, toolArguments.length);
        Process process = new ProcessBuilder(command).redirectErrorStream(true).start();
        // Helpers emit bounded output. Wait before reading so EOF cannot bypass the deadline.
        if (!process.waitFor(35, TimeUnit.SECONDS)) {
            process.destroyForcibly();
            throw new IllegalStateException("Native helper timed out");
        }
        String output = new String(process.getInputStream().readAllBytes(), StandardCharsets.UTF_8);
        require(process.exitValue() == 0, oneLine(output));
        return output.trim();
    }

    private static void waitForFile(Path path, Duration timeout) throws Exception {
        Instant deadline = Instant.now().plus(timeout);
        while (Instant.now().isBefore(deadline)) {
            if (Files.isRegularFile(path)) {
                return;
            }
            Thread.sleep(10);
        }
        throw new IllegalStateException("Backend stop acknowledgement timed out");
    }

    private static void waitForTermination(GdbLaunch launch, Duration timeout)
            throws InterruptedException {
        Instant deadline = Instant.now().plus(timeout);
        while (Instant.now().isBefore(deadline)) {
            if (launch.isTerminated()) {
                return;
            }
            Thread.sleep(20);
        }
        throw new IllegalStateException("CubeIDE launch did not terminate");
    }

    private static long word(byte[] bytes, int offset) {
        return (bytes[offset] & 0xffL) | ((bytes[offset + 1] & 0xffL) << 8)
            | ((bytes[offset + 2] & 0xffL) << 16) | ((bytes[offset + 3] & 0xffL) << 24);
    }

    private static String hex(byte[] bytes) {
        StringBuilder result = new StringBuilder();
        for (byte value : bytes) {
            result.append(String.format("%02x", value & 0xff));
        }
        return result.toString();
    }

    private static String oneLine(String value) {
        return value == null ? "null" : value.replace('\r', ' ').replace('\n', ' ').trim();
    }

    private static Map<String, String> parseArguments(String[] arguments) {
        Map<String, String> result = new HashMap<>();
        for (int index = 0; index + 1 < arguments.length; index += 2) {
            result.put(arguments[index], arguments[index + 1]);
        }
        return result;
    }

    private static String required(Map<String, String> arguments, String name) {
        String value = arguments.get(name);
        if (value == null || value.isBlank()) {
            throw new IllegalArgumentException("Missing " + name + " argument");
        }
        return value;
    }

    private static void require(boolean condition, String message) {
        if (!condition) {
            throw new IllegalStateException(message);
        }
    }

    @Override
    public void stop() {
    }
}

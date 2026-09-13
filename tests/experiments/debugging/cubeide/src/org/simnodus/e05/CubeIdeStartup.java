package org.simnodus.e05;

import java.io.PrintStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;

import org.eclipse.ui.IStartup;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.preferences.ConfigurationScope;
import org.eclipse.ui.PlatformUI;

public final class CubeIdeStartup implements IStartup {
    @Override
    public void earlyStartup() {
        String propertiesPath = System.getenv("SIMNODUS_E05_PROPERTIES");
        if (propertiesPath == null || propertiesPath.isBlank()) {
            return;
        }
        Path path = Path.of(propertiesPath);
        try {
            Properties properties = new Properties();
            try (var reader = Files.newBufferedReader(path)) {
                properties.load(reader);
            }
            Map<String, String> arguments = new HashMap<>();
            for (String name : properties.stringPropertyNames()) {
                arguments.put("--" + name, properties.getProperty(name));
            }
            Path output = Path.of(arguments.get("--simnodus-output"));
            try (PrintStream stream = new PrintStream(
                    Files.newOutputStream(output), true, "UTF-8")) {
                boolean skipSuggestion = Platform.getPreferencesService().getBoolean(
                    "org.eclipse.ui", "windows.defender.startup.check.skip", false, null);
                boolean disableAnalytics = java.util.Arrays.asList(Platform.getApplicationArgs()).contains("-disableAnalytics");
                var documentationVersion = Platform.getBundle("com.st.stm32cube.ide.documentation").getVersion();
                String releaseNotesKey = documentationVersion.getMajor() + "."
                    + documentationVersion.getMinor() + "." + documentationVersion.getMicro();
                boolean releaseNotesSeen = ConfigurationScope.INSTANCE
                    .getNode("com.st.stm32cube.ide.documentation").getBoolean(releaseNotesKey, false);
                if (!skipSuggestion || !disableAnalytics || !releaseNotesSeen) {
                    throw new IllegalStateException("Disposable startup configuration was not applied");
                }
                stream.println("STARTUP_POLICY defenderSuggestionSkipped=true analyticsDisabled=true"
                    + " releaseNotesSeen=true documentationVersion=" + releaseNotesKey);
                CubeIdeRunner.run(arguments, stream);
            }
        } catch (Throwable error) {
            Path failure = path.resolveSibling("cubeide-startup-failure.txt");
            try (PrintStream stream = new PrintStream(
                    Files.newOutputStream(failure), true, "UTF-8")) {
                error.printStackTrace(stream);
            } catch (Exception ignored) {
                error.printStackTrace();
            }
        } finally {
            PlatformUI.getWorkbench().getDisplay().asyncExec(() -> {
                Path shutdown = path.resolveSibling("cubeide-shutdown.txt");
                try {
                    boolean accepted = PlatformUI.getWorkbench().close();
                    Files.writeString(shutdown, "accepted=" + accepted);
                } catch (Throwable error) {
                    try (PrintStream stream = new PrintStream(Files.newOutputStream(shutdown), true, "UTF-8")) {
                        error.printStackTrace(stream);
                    } catch (Exception loggingError) {
                        loggingError.printStackTrace();
                    }
                }
            });
        }
    }
}

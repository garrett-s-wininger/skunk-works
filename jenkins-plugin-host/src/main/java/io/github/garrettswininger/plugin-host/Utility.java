package io.github.garrettswininger.pluginhost;

import hudson.PluginWrapper;
import java.io.File;
import jenkins.model.Jenkins;

class Utility {
    static File getDataDirectory() {
        var jenkins = Jenkins.get();
        var wrapper = jenkins.getPluginManager().getPlugin(PluginHost.class);
        var rootDir = jenkins.getRootDir();

        return new File(rootDir, wrapper.getShortName());
    }

    static File getAutoloadDirectory() {
        return new File(Utility.getDataDirectory(), "autoload");
    }
}

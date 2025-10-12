package io.github.garrettswininger.hosting;

import hudson.ExtensionPoint;
import java.lang.reflect.InvocationTargetException;

public abstract class DynamicPlugin<T extends ExtensionPoint, U extends T> {
    public final Class<T> extension;
    public final Class<U> implementation;

    public DynamicPlugin(Class<T> extension, Class<U> implementation) {
        this.extension = extension;
        this.implementation = implementation;
    }

    public U getInstance() throws IllegalAccessException, InvocationTargetException,
            InstantiationException, NoSuchMethodException {
        return this.implementation.getDeclaredConstructor().newInstance();
    }
}

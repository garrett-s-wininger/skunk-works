package io.github.garrettswininger.pluginhost;

import java.util.ArrayList;
import java.util.List;
import jenkins.model.Jenkins;
import org.jenkinsci.plugins.workflow.steps.Step;
import org.jenkinsci.plugins.workflow.steps.StepDescriptor;

class StepDescriptorAdapter implements ExtensionAdapter<StepDescriptor, StepDescriptor> {
  @Override
  public Class<StepDescriptor> extensionType() {
    return StepDescriptor.class;
  }

  @Override
  @SuppressWarnings("unchecked")
  public List<StepDescriptor> registerStable(List<StepDescriptor> delegates) {
    final var stepDescriptorList = StepDescriptor.all();
    final var descriptorList = Jenkins.get().getDescriptorList(Step.class);
    final List<StepDescriptor> registered = new ArrayList<>();

    delegates.forEach(
        delegate -> {
          if (!stepDescriptorList.contains(delegate)) {
            stepDescriptorList.add(delegate);
          }

          if (!descriptorList.contains(delegate)) {
            descriptorList.add(delegate);
          }

          registered.add(delegate);
        });

    return registered;
  }

  @Override
  @SuppressWarnings("unchecked")
  public void swap(List<StepDescriptor> existing, List<StepDescriptor> delegates) {
    if (existing.size() != delegates.size()) {
      throw new IllegalStateException(
          String.format(
              "Wrapper/delegate count mismatch for StepDescriptor reload (%d != %d)",
              existing.size(), delegates.size()));
    }

    final var stepDescriptorList = StepDescriptor.all();
    final var descriptorList = Jenkins.get().getDescriptorList(Step.class);

    for (int i = 0; i < existing.size(); i++) {
      final var next = delegates.get(i);
      if (!stepDescriptorList.contains(next)) {
        stepDescriptorList.add(next);
      }

      if (!descriptorList.contains(next)) {
        descriptorList.add(next);
      }

      final var previous = existing.set(i, next);
      stepDescriptorList.remove(previous);
      descriptorList.remove(previous);
    }
  }

  @Override
  @SuppressWarnings("unchecked")
  public void deregister(List<StepDescriptor> wrappers) {
    final var stepDescriptorList = StepDescriptor.all();
    final var descriptorList = Jenkins.get().getDescriptorList(Step.class);

    wrappers.forEach(
        descriptor -> {
          stepDescriptorList.remove(descriptor);
          descriptorList.remove(descriptor);
        });
  }
}

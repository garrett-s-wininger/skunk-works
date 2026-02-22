package io.github.garrettswininger.examplehosted;

import io.github.garrettswininger.hosting.DynamicPlugin;
import io.github.garrettswininger.hosting.Hosted;
import org.jenkinsci.plugins.workflow.steps.StepDescriptor;

@Hosted
public class HostedEchoStep
    extends DynamicPlugin<StepDescriptor, HostedEchoStepDefinition.DescriptorImpl> {
  public HostedEchoStep() {
    super(StepDescriptor.class, HostedEchoStepDefinition.DescriptorImpl.class);
  }
}

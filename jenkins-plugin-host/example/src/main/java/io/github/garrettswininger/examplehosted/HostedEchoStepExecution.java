package io.github.garrettswininger.examplehosted;

import hudson.model.TaskListener;
import org.jenkinsci.plugins.workflow.steps.SynchronousStepExecution;

public class HostedEchoStepExecution extends SynchronousStepExecution<String> {
  private static final long serialVersionUID = 1L;

  private final transient HostedEchoStepDefinition step;

  public HostedEchoStepExecution(
      HostedEchoStepDefinition step, org.jenkinsci.plugins.workflow.steps.StepContext context) {
    super(context);
    this.step = step;
  }

  @Override
  protected String run() throws Exception {
    final var listener = getContext().get(TaskListener.class);

    if (listener != null) {
      listener.getLogger().printf("[HostedEchoStep - Modified] %s%n", step.getMessage());
    }

    return step.getMessage();
  }
}

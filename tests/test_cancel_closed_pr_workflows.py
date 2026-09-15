from pathlib import Path
import unittest


WORKFLOW = Path(__file__).resolve().parents[1] / ".github/workflows/cancel-closed-pr-workflows.yml"


class CancelClosedPrWorkflowsTests(unittest.TestCase):
    def setUp(self):
        self.workflow = WORKFLOW.read_text()

    def test_runs_only_after_a_pull_request_closes(self):
        self.assertIn("pull_request:\n    types: [closed]", self.workflow)

    def test_has_only_the_actions_write_permission_needed_to_cancel(self):
        self.assertIn("permissions:\n  actions: write", self.workflow)

    def test_limits_cancellation_to_active_runs_for_the_closed_pr(self):
        self.assertIn("for status in queued in_progress", self.workflow)
        self.assertIn("event=pull_request&status=${status}", self.workflow)
        self.assertIn(".number == (env.PR_NUMBER | tonumber)", self.workflow)
        self.assertIn(".id != (env.GITHUB_RUN_ID | tonumber)", self.workflow)
        self.assertIn("actions/runs/${run_id}/cancel", self.workflow)


if __name__ == "__main__":
    unittest.main()

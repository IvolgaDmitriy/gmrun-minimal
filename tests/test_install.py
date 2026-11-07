import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Installation(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.home = Path(self.temporary.name) / "home with spaces"
        self.home.mkdir()
        self.config = self.home / "custom config"
        self.env = dict(os.environ, HOME=str(self.home),
                        XDG_CONFIG_HOME=str(self.config),
                        XDG_STATE_HOME=str(self.home / "custom state"))

    def run_installer(self, action):
        subprocess.run([sys.executable, str(ROOT / "install.py"), action],
                       env=self.env, check=True, capture_output=True, text=True)

    def test_install_and_remove_preserve_history(self):
        history = self.home / ".gmrun_history"
        history.write_text("nautilus\n")
        self.run_installer("install")
        launcher = self.home / ".local/bin/gmrun-minimal"
        self.assertTrue(os.access(launcher, os.X_OK))
        self.assertTrue((self.config / "gmrun-minimal/gtkrc").is_file())
        self.run_installer("uninstall")
        self.assertFalse(launcher.exists())
        self.assertFalse((self.home / ".gmrunrc").exists())
        self.assertEqual(history.read_text(), "nautilus\n")

    def test_reinstall_keeps_edits_and_restores_original(self):
        config = self.home / ".gmrunrc"
        config.write_text("Width = 700\n")
        self.run_installer("install")
        config.write_text("Width = 900\n")
        theme = self.config / "gmrun-minimal/gtkrc"
        theme.write_text('gtk-font-name = "Sans 18"\n')
        self.run_installer("install")
        self.assertEqual(config.read_text(), "Width = 900\n")
        self.assertEqual(theme.read_text(), 'gtk-font-name = "Sans 18"\n')
        self.run_installer("uninstall")
        self.assertEqual(config.read_text(), "Width = 700\n")

    def test_symlink_is_restored_without_modifying_its_target(self):
        original = self.home / "original-config"
        original.write_text("Width = 640\n")
        config = self.home / ".gmrunrc"
        config.symlink_to(original.name)
        self.run_installer("install")
        self.assertFalse(config.is_symlink())
        self.assertEqual(original.read_text(), "Width = 640\n")
        self.run_installer("uninstall")
        self.assertTrue(config.is_symlink())
        self.assertEqual(config.readlink(), Path(original.name))
        self.assertEqual(config.read_text(), "Width = 640\n")


if __name__ == "__main__":
    unittest.main()

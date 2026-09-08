"""Check tracked publication files and local Markdown links with no dependencies."""
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit


def forbidden_path(path: str) -> bool:
    item = PurePosixPath(path.lower())
    return (any(part in {".tools", ".tmp", "build", "dist", "artifacts", ".mousewheel"} for part in item.parts)
            or item.name.startswith(".env")
            or item.name in {"config.json", "config.json.lock", "id_rsa", "id_ed25519", "agents.md", "agents.custome.md"}
            or item.suffix in {".wav", ".ape", ".flac", ".mp3", ".exe", ".dll", ".zip", ".pdb", ".pem", ".pfx", ".key"})


def validate(root: Path, paths: list[str]) -> list[str]:
    errors = []
    tracked = set(paths)
    for name in paths:
        if forbidden_path(name):
            errors.append(f"{name}: private, generated or unrelated file must not be tracked")
        if not name.endswith(".md"):
            continue
        for target in re.findall(r"\]\(([^)]+)\)", (root / name).read_text(encoding="utf-8")):
            url = urlsplit(target)
            if url.scheme or url.netloc or not url.path:
                continue
            resolved = (root / name).parent.joinpath(unquote(url.path)).resolve()
            if not resolved.is_relative_to(root.resolve()) or resolved.relative_to(root.resolve()).as_posix() not in tracked:
                errors.append(f"{name}: missing local target {target}")
    return errors


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    result = subprocess.run(["git", "ls-files", "-z"], cwd=root, capture_output=True, check=True)
    paths = [p for p in result.stdout.decode("utf-8").split("\0") if p]
    errors = validate(root, paths)
    print("\n".join(errors) if errors else f"Repository checks passed ({len(paths)} tracked files)")
    sys.exit(bool(errors))

#!/usr/bin/env python3
import os, sys, json, subprocess, re
from typing import Dict, Tuple, Callable, Optional

def call(cmd) -> str:
	sys.stdout.flush()
	ret = subprocess.run(cmd, check=True, stdout=subprocess.PIPE, text=True)
	return ret.stdout

lint_rules: Dict[str, Tuple[Callable, str]] = {}

def lint_rule(description: str):
	def f(func):
		assert func.__name__ not in lint_rules.keys()
		lint_rules[func.__name__] = (func, description)
	return f

def get_commit_range() -> Optional[str]:
	if len(sys.argv) > 1:
		return sys.argv[1]
	# https://github.com/actions/runner/issues/342#issuecomment-590670059
	event_name = os.environ["GITHUB_EVENT_NAME"]
	with open(os.environ["GITHUB_EVENT_PATH"], "rb") as f:
		event = json.load(f)
	if event_name == "push":
		if event["created"] or event["forced"]:
			print("Skipping logic on branch creation or force-push")
			return None
		return event["before"] + "..." + event["after"]
	elif event_name == "pull_request":
		return event["pull_request"]["base"]["sha"] + ".." + event["pull_request"]["head"]["sha"]
	return None

def do_lint(commit_range: str) -> bool:
	commits = call(["git", "log", "--pretty=format:%H %s", commit_range]).splitlines()
	print(f"Linting {len(commits)} commit(s):")
	any_failed = False
	for commit in commits:
		sha, _, _ = commit.partition(' ')
		#print(commit)
		body = call(["git", "show", "-s", "--format=%B", sha]).splitlines()
		failed = []
		if len(body) == 0:
			failed.append("* Commit message must not be empty")
		else:
			for k, v in lint_rules.items():
				if not v[0](body):
					failed.append(f"* {v[1]} [{k}]")
		if failed:
			any_failed = True
			print("-" * 40)
			sys.stdout.flush()
			subprocess.run(["git", "-P", "show", "-s", sha])
			print("\nhas the following issues:")
			print("\n".join(failed))
			print("-" * 40)
	return any_failed

################################################################################

NO_PREFIX_WHITELIST = r"^Revert \"(.*)\"|^Reapply \"(.*)\"|^Release [0-9]|^Update VERSION$"

@lint_rule("Subject line must contain a prefix identifying the sub system")
def subsystem_prefix(body):
	if re.search(NO_PREFIX_WHITELIST, body[0]):
		return True
	m = re.search(r"^([^:]+): ", body[0])
	if not m:
		return False
	# a comma-separated list is okay
	s = re.sub(r", ", "", m.group(1))
	# but no spaces otherwise
	return not " " in s

@lint_rule("First word after : must be lower case")
def description_lowercase(body):
	# Allow all caps for acronyms and options with --
	return (re.search(NO_PREFIX_WHITELIST, body[0]) or
			re.search(r": (?:[A-Z]{2,} |--[a-z]|[a-z0-9])", body[0]))

@lint_rule("Subject line must not end with a full stop")
def no_dot(body):
	return not body[0].rstrip().endswith('.')

@lint_rule("There must be an empty line between subject and extended description")
def empty_line(body):
	return len(body) == 1 or body[1].strip() == ""

# been seeing this one all over github lately, must be the webshits
@lint_rule("Do not use 'conventional commits' style")
def no_cc(body):
	return not re.search(r"(?i)^(feat|fix|chore|refactor)[!:(]", body[0])

@lint_rule("History must be linear, no merge commits")
def no_merge(body):
	return not body[0].startswith("Merge ")

@lint_rule("Subject line should be shorter than 72 characters")
def line_too_long(body):
	revert = re.search(r"^Revert \"(.*)\"|^Reapply \"(.*)\"", body[0])
	return True if revert else len(body[0]) <= 72

@lint_rule("Prefix should not include C file extensions (use `vo_gpu: ...` not `vo_gpu.c: ...`)")
def no_file_exts(body):
	return not re.search(r"[a-z0-9]\.[ch]: ", body[0])

################################################################################

if __name__ == "__main__":
	commit_range = get_commit_range()
	if commit_range is None:
		exit(0)
	print("Commit range:", commit_range)
	any_failed = do_lint(commit_range)
	exit(1 if any_failed else 0)

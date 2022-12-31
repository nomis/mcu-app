.PHONY: clean

PIPENV_VENV_IN_PROJECT=1

.venv/bin/python: Pipfile
	rm -rf .venv
	pipenv install --skip-lock

clean:
	rm -rf .venv

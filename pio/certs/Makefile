.PHONY: all clean pipenv
.DELETE_ON_ERROR:

PIPENV=$(CURDIR)/../pipenv
PYTHON=$(PIPENV)/.venv/bin/python3

all: x509_crt_bundle

x509_crt_bundle: $(wildcard *.der) $(wildcard *.pem) \
		$(wildcard ../../../certs/*.der) \
		$(wildcard ../../../certs/*.pem) | pipenv
	$(PYTHON) ../esp-gen_crt_bundle.py -i $^

clean:
	+$(MAKE) -C $(PIPENV) clean
	rm -f x509_crt_bundle

pipenv:
	+$(MAKE) -C $(PIPENV) -L

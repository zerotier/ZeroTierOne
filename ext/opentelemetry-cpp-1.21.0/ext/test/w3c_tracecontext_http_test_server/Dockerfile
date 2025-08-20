# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

FROM python

RUN pip install aiohttp
RUN git clone https://github.com/w3c/trace-context

WORKDIR ./trace-context/test

ENTRYPOINT [ "python", "test.py" ]

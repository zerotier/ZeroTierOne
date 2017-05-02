Root Server Watcher
======

This is a small daemon written in NodeJS that watches a set of root servers and records peer status information into a Postgres database.

To use type `npm install` to install modules. Then edit `config.json.example` and rename to `config.json`. For each of your roots you will need to configure a way for this script to reach it. You will also need to use `schema.sql` to initialize a Postgres database to contain your logs and set it up in `config.json` as well.

This doesn't (yet) include any software for reading the log database and doing anything useful with the information inside, though given that it's a simple SQL database it should not be hard to compose queries to show interesting statistics.

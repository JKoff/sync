Motivation
----------

Deploys take way too long. `aws s3` performs a full scan to figure out what has changed. Then the actual transfers must take place, possibly consisting of many files. And this all happens in a blocking fashion, once the changes are ready, as opposed to work being done in the background before it is needed.

This is a risk (can't push e.g. an urgent bugfix as quickly), a waste of time (ever wait for a deploy before getting back to work?), and encourages bad habits (avoiding the slow step whenever possible).

"Sync" sets out to improve the situation in whatever ways are possible.


What's missing
--------------

- More advanced planning. Currently only fan-out is supported.


Example usage
-----

On primary:
> sync-primary inst1 <32 random chars> --replica=123.45.67.89:1234 --exclude=^.git

On client:
> sync-replica inst2 <32 random chars> 123.45.67.89 1234

NOT SUPPORTED YET:
Then after making file changes:
> syncctl flush
To ensure that changes have been propagated to replicas.
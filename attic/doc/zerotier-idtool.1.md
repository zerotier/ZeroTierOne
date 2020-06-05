zerotier-idtool(1) -- tool for creating and manipulating ZeroTier identities
============================================================================

## SYNOPSIS

`zerotier-idtool` <command> [args]

## DESCRIPTION

**zerotier-idtool** is a command line utility for doing things with ZeroTier identities. A ZeroTier identity consists of a public/private key pair (or just the public if it's only an identity.public) and a 10-digit hexadecimal ZeroTier address derived from the public key by way of a proof of work based hash function.

## COMMANDS

When command arguments call for a public or secret (full) identity, the identity can be specified as a path to a file or directly on the command line.

 * `help`:
   Display help. (Also running with no command does this.)

 * `generate` [secret file] [public file] [vanity]:
   Generate a new ZeroTier identity. If a secret file is specified, the full identity including the private key will be written to this file. If the public file is specified, the public portion will be written there. If no file paths are specified the full secret identity is output to STDOUT. The vanity prefix is a series of hexadecimal digits that the generated identity's address should start with. Typically this isn't used, and if it's specified generation can take a very long time due to the intrinsic cost of generating identities with their proof of work function. Generating an identity with a known 16-bit (4 digit) prefix on a 2.8ghz Core i5 (using one core) takes an average of two hours.

 * `validate` <identity, only public part required>:
   Locally validate an identity's key and proof of work function correspondence.

 * `getpublic` <full identity with secret>:
   Extract the public portion of an identity.secret and print to STDOUT.

 * `sign` <full identity with secret> <file to sign>:
   Sign a file's contents with SHA512+ECC-256 (ed25519). The signature is output in hex to STDOUT.

 * `verify` <identity, only public part required> <file to check> <signature in hex>:
   Verify a signature created with `sign`.

 * `mkcom` <full identity with secret> [id,value,maxdelta] [...]:
   Create and sign a network membership certificate. This is not generally useful since network controllers do this automatically and is included mostly for testing purposes.

## EXAMPLES

Generate and dump a new identity:

    $ zerotier-idtool generate

Generate and write a new identity, both secret and public parts:

    $ zerotier-idtool generate identity.secret identity.public

Generate a vanity address that begins with the hex digits "beef" (this will take a while!):

    $ zerotier-idtool generate beef.secret beef.public beef

Sign a file with an identity's secret key:

    $ zerotier-idtool sign identity.secret last_will_and_testament.txt

Verify a file's signature with a public key:

    $ zerotier-idtool verify identity.public last_will_and_testament.txt

## COPYRIGHT

(c)2011-2016 ZeroTier, Inc. -- https://www.zerotier.com/ -- https://github.com/zerotier

## SEE ALSO

zerotier-one(8), zerotier-cli(1)

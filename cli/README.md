The new ZeroTier CLI!
====

With this update we've expanded upon the previous CLI's functionality, so things should seem pretty familiar. Here are some of the new features we've introduced:

 - Create and administer networks on ZeroTier Central directly from the console.
 - Service configurations, allows you to control local/remote instances of ZeroTier One
 - Identity generation and management is now part of the same CLI tool

***
## Configurations

Configurations are a way for you to nickname and logically organize the control of ZeroTier services running locally or remotely (this includes ZeroTier Central!). They're merely groupings of service API url's and auth tokens. The CLI's settings data is contained within `.zerotierCliSettings`.

For instance, you can control your local instance of ZeroTier One via the `@local` config. By default it is represented as follows:

```
"local": {
      "auth": "7tyqRoFytajf21j2l2t9QPm5",
      "type": "one",
      "url": "http://127.0.0.1:9993/"
}
``` 

As an example, if you issue the command `zerotier ls` is it implicitly stating `zerotier @local ls`. 

With the same line of thinking, you could create a `@my.zerotier.com` which would allow for something like `zerotier @my.zerotier.com net-create` which talks to our hosted ZeroTier Central to create a new network.



## Command families 

- `cli-` is for configuring the settings data for the CLI itself, such as adding/removing `@thing` configurations, variables, etc.
- `net-` is for operating on a *ZeroTier Central* service such as `https://my.zerotier.com`
- `id-` is for handling ZeroTier identities.

And those commands with no prefix are there to allow you to operate ZeroTier One instances either local or remote.

***
## Useful command examples

*Add a ZeroTier One configuration:*

 - `zerotier cli-add-zt MyLocalConfigName https://127.0.0.1:9993/ <authtoken>`

*Add a ZeroTier Central configuration:*

 - `zerotier cli-add-central MyZTCentralConfigName https://my.zerotier.com/ <centralAPIAuthtoken>`

*Set a default ZeroTier One instance:*

 - `zerotier cli-set defaultOne MyLocalConfigName`

*Set a default ZeroTier Central:*

 - `zerotier cli-set defaultCentral MyZTCentralConfigName`


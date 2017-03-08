# mruby-actor
Breaking changes
================
The api has been rewritten, more to come.


Preliminary
===========

mruby-actor is a library to bring distributed concurrency to mruby with the [actor model](https://en.wikipedia.org/wiki/Actor_model).
If you just need local threading based on the actor model for mruby take a look at [mruby-zmq](https://github.com/Asmod4n/mruby-zmq)

Prerequirements
===============
You need to have [libzmq](https://github.com/zeromq/libzmq) with draft methods and [zyre](https://github.com/zeromq/zyre) installed on your system


Blocking operations
===================

Blocking operations must be avoided at all costs. If you really need to block a mrb context do so in a ZMQ::Thread.

Security
========

All communication between mruby actors is encrypted, except the Service Discovery based on [zyre](https://github.com/zeromq/zyre), feel free to add a PR for it at zyre.

Examples
========
```ruby
actor = Actor.new(remote_server_endpoint: "tcp://en0:*")
actor2 = Actor.new(remote_server_endpoint: "tcp://en0:*")
actor3 = Actor.new(remote_server_endpoint: "tcp://en0:*")
string = actor2.remote_new(actor2.remote_actors.sample[:peerid], String, "hallo")
string.async(:upcase!)
string.to_str
```

License
=======
   Copyright 2015,2017 Hendrik Beskow

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this project except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

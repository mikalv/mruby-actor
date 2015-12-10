# mruby-actor
Preliminary
===========

mruby-actor is a library to bring distributed (and local) concurrency to mruby with the [actor model](https://en.wikipedia.org/wiki/Actor_model).

Prerequirements
===============
You need to have czmq and zyre installed on your system, build and installation instructions: https://github.com/zeromq/czmq#building-and-installing

https://github.com/zeromq/zyre has similar installation instructions.


Examples
========
```ruby
actor = Actor.new
string = actor.init(String, "i am a very long string")
puts string.send(:to_s)
```

with a compiled ruby file
-------------------------
worker.rb:
```ruby
class Worker
  def heavy_lifting
    CZMQ::Zclock.sleep 5000
    puts "finished"
  end
end
```
---------
```shell
mruby/bin/mrbc worker.rb
```
--------
```ruby
actor = Actor.new

actor.load_irep_file('worker.mrb')

worker = actor.init('Worker')

worker.send(:heavy_lifting)

worker.async_send(:heavy_lifting)

```

Actor Discovery
===============

Actors can be spawned with and without being able to be discovered, we use https://github.com/zeromq/zyre for that.

Discovery in the same process
-----------------------------
```ruby

disc1 = Actor.new(name: "disc1")
disc1.zyre_endpoint = "inproc://1"
disc1.zyre_gossip_bind("inproc://zyre") #at least one actor must bind to a known endpoint, so discovery can work.
disc1.zyre_start

disc2 = Actor.new(name: "disc2")
disc2.zyre_endpoint = "inproc://2"
disc2.zyre_gossip_connect("inproc://zyre")
disc2.zyre_start

string = disc2.remote_init("disc1", String, "foo bar")
puts disc2.remote_actors

puts string.send(:to_s)

```

Discovery via tcp
-----------------
```ruby
disc1 = Actor.new(name: "disc1", router_endpoint: "tcp://127.0.0.1:*", pull_endpoint: "tcp://127.0.0.1:*")
disc1.zyre_endpoint = "tcp://127.0.0.1:5001"
disc1.zyre_gossip_bind("tcp://127.0.0.1:5002") #at least one actor must bind to a known endpoint, so discovery can work.
disc1.zyre_start

disc2 = Actor.new(name: "disc2", router_endpoint: "tcp://127.0.0.1:*", pull_endpoint: "tcp://127.0.0.1:*")
disc2.zyre_endpoint = "tcp://127.0.0.1:5003"
disc2.zyre_gossip_connect("tcp://127.0.0.1:5002")
disc2.zyre_start

CZMQ::Zclock.sleep(100) # give them some time to find eachother

string = disc2.remote_init("disc1", String, "foo bar")
puts disc2.remote_actors

puts string.send(:to_s)
```

Blocking operations
===================

Blocking operations should be avoided at all costs and are forbidden in Actors which can be discovered.

If you absoluteley have to use blocking operations, don't ever send any messages to that actor again, in case of Actors which can be discovered this is not possible because other actors automatically send messages.

Security
========

At the moment, anyone who can connect to a actor endpoint can execute anything on that actor, this will be addressed once https://github.com/zeromq/zyre has authentication, feel free to open a PR for it.

Mruby is quite interesting when it comes to Security, when you for example never want to use eval, simple don't compile it with support for it.

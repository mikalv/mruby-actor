actor = Actor.new(name: "actor1", zyre_endpoint: "inproc://1", zyre_gossip_bind: "inproc://zyre", zyre_start: 1)
actor2 = Actor.new(name: "actor2", zyre_endpoint: "inproc://2", zyre_gossip_connect: "inproc://zyre", zyre_start: 1)

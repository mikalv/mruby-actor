unless ZMQ.const_defined?("Poller")
  raise RuntimeError, "mruby-actor needs libzmq with poller support"
end

unless LibZMQ.has? "curve"
  raise RuntimeError, "mruby-actor needs libzmq with curve support"
end

class Actor < ZMQ::Thread
  include ZMQ::ThreadConstants

  REMOTE_NEW = 4.freeze
  REMOTE_SEND = 5.freeze
  REMOTE_ASYNC = 6.freeze
  REMOTE_ACTORS = 7.freeze

  ACTORS = 3.freeze

  def self.new(options = {}, &block) # this starts the background thread
    super(Actor_fn, options, &block)
  end

  def remote_new(peerid, mrb_class, *args, &block)
    LibZMQ.send(@pipe, [REMOTE_NEW, peerid, mrb_class, args, block].to_msgpack, 0)
    msg = MessagePack.unpack(@pipe.recv.to_str(true))
    case msg[TYPE]
    when INSTANCE
      RemoteThreadProxy.new(self, peerid, msg[1])
    when EXCEPTION
      raise msg[1]
    end
  end

  def remote_send(peerid, object_id, method, *args, &block)
    LibZMQ.send(@pipe, [REMOTE_SEND, peerid, object_id, method, args, block].to_msgpack, 0)
    msg = MessagePack.unpack(@pipe.recv.to_str(true))
    case msg[TYPE]
    when RESULT
      msg[1]
    when EXCEPTION
      raise msg[1]
    end
  end

  def remote_async(peerid, object_id, method, *args, &block)
    LibZMQ.send(@pipe, [REMOTE_ASYNC, peerid, object_id, method, args, block].to_msgpack, 0)
  end

  REMOTE_ACTORS_MSG = [REMOTE_ACTORS].to_msgpack.freeze

  def remote_actors
    LibZMQ.send(@pipe, REMOTE_ACTORS_MSG, 0)
    msg = MessagePack.unpack(@pipe.recv.to_str(true))
    case msg[TYPE]
    when ACTORS
      msg[1]
    when EXCEPTION
      raise msg[1]
    end
  end

  class RemoteThreadProxy
    attr_reader :object_id

    def initialize(thread, peerid, object_id)
      @thread = thread
      @peerid = peerid
      @object_id = object_id
    end

    def send(m, *args, &block)
      @thread.remote_send(@peerid, @object_id, m, *args, &block)
    end

    def async(m, *args, &block)
      @thread.remote_async(@peerid, @object_id, m, *args, &block)
      self
    end

    def respond_to?(m)
      super(m) || @thread.remote_send(@peerid, @object_id, :respond_to?, m)
    end

    def method_missing(m, *args, &block)
      @thread.remote_send(@peerid, @object_id, m, *args, &block)
    end
  end

  class Actor_fn < ZMQ::Thread::Thread_fn # this is the background thread
    include ZMQ::ThreadConstants

    ROUTER_ENDPOINT = 'router_endpoint'.freeze
    ROUTER_PUBLIC_KEY = 'router_public_key'.freeze
    VERSION = '3'

    def initialize(options, &block) # these are the options passed to the frontend Actor constructor, they are saved as @options
      super
      unless @auth
        raise ArgumentError, ":auth Hash missing, cannot start background Actor"
      end
      server_keypair = @options.fetch(:server_keypair) { LibZMQ.curve_keypair }
      @client_keypair = @options.fetch(:client_keypair) { LibZMQ.curve_keypair }
      @remote_server = ZMQ::Router.new
      @poller << @remote_server
      @group = sprintf('%s-%s', self.class.name, @options.fetch(:version, VERSION))
      @remote_server.curve_security(type: :server, secret_key: server_keypair.fetch(:secret_key), zap_domain: @group)
      @remote_server.bind(@options.fetch(:remote_server_endpoint) { sprintf('tcp://%s:*', ZMQ.network_interfaces.first) } )
      last_endpoint = @remote_server.last_endpoint
      @remote_server.identity = last_endpoint
      @discovery = Zyre.new
      @poller << @discovery
      @discovery[ROUTER_ENDPOINT] = last_endpoint
      @discovery[ROUTER_PUBLIC_KEY] = server_keypair.fetch(:public_key)
      @discovery.join(@group)
      @remote_clients = {}
      @discovery.start
    end

    def run
      until @interrupted
        @poller.wait do |socket, events|
          case socket
          when @pipe
            handle_pipe
          when @auth
            @auth.handle_zap
          when @discovery
            handle_discovery
          when @remote_server
            handle_server
          end
        end
      end
    end

    def remote_client(peerid)
      @remote_clients.fetch(peerid) do
        client = ZMQ::Dealer.new
        client.curve_security(type: :client, server_key: @discovery.peer_header_value(peerid, ROUTER_PUBLIC_KEY),
          public_key: @client_keypair.fetch(:public_key), secret_key: @client_keypair.fetch(:secret_key))
        client.rcvtimeo = 120000
        client.sndtimeo = 120000
        client.connect(@discovery.peer_header_value(peerid, ROUTER_ENDPOINT))
        @remote_clients[peerid] = client
        client
      end
    end

    def remote_actors
      actors = []
      if (peers = @discovery.peers_by_group(@group))
        peers.each do |peerid|
          actors << {
            peerid: peerid,
            router_endpoint: @discovery.peer_header_value(peerid, ROUTER_ENDPOINT),
            router_public_key: @discovery.peer_header_value(peerid, ROUTER_PUBLIC_KEY)
          }
        end
      end
      actors
    end

    def handle_pipe
      msg = @pipe.recv
      msg_str = msg.to_str(true)
      if msg_str == TERM
        @interrupted = true
      else
        begin
          msg_val = MessagePack.unpack(msg_str)
          case msg_val[TYPE]
          when NEW
            instance = msg_val[1].new(*msg_val[2], &msg_val[3])
            id = instance.__id__
            LibZMQ.send(@pipe, [INSTANCE, id].to_msgpack, 0)
            @instances[id] = instance
          when SEND
            LibZMQ.send(@pipe, [RESULT, @instances[msg_val[1]].__send__(msg_val[2], *msg_val[3], &msg_val[4])].to_msgpack, 0)
          when ASYNC
            begin
              @instances[msg_val[1]].__send__(msg_val[2], *msg_val[3], &msg_val[4])
            rescue LibZMQ::ETERMError => e
              raise e
            rescue => e
              ZMQ.logger.crash(e)
            end
          when FINALIZE
            @instances.delete(msg_val[1])
          when REMOTE_NEW, REMOTE_SEND
            client = remote_client(msg_val[1])
            LibZMQ.msg_send(msg, client, 0)
            LibZMQ.msg_send(client.recv, @pipe, 0)
          when REMOTE_ASYNC
            LibZMQ.msg_send(msg, remote_client(msg_val[1]), 0)
          when REMOTE_ACTORS
            LibZMQ.send(@pipe, [ACTORS, remote_actors].to_msgpack, 0)
          end
        rescue => e
          LibZMQ.send(@pipe, [EXCEPTION, e].to_msgpack, 0)
        end
      end
    rescue => e
      ZMQ.logger.crash(e)
    end

    EXIT = 'EXIT'.freeze

    def handle_discovery
      event, peerid, *_ = @discovery.recv
      if event == EXIT
        @remote_clients.delete(peerid)
      end
    rescue => e
      ZMQ.logger.crash(e)
    end

    def handle_server
      peer, msg = @remote_server.recv
      begin
        msg = MessagePack.unpack(msg.to_str(true))
        case msg.fetch(TYPE)
        when REMOTE_NEW
          instance = msg.fetch(2).new(*msg.fetch(3), &msg[4])
          id = instance.__id__
          instance_msg = [INSTANCE, id].to_msgpack
          LibZMQ.msg_send(peer, @remote_server, LibZMQ::SNDMORE)
          LibZMQ.send(@remote_server, instance_msg, 0)
          @instances[id] = instance
        when REMOTE_SEND
          result = [RESULT, @instances.fetch(msg.fetch(2)).__send__(msg.fetch(3), *msg.fetch(4), &msg[5])].to_msgpack
          LibZMQ.msg_send(peer, @remote_server, LibZMQ::SNDMORE)
          LibZMQ.send(@remote_server, result, 0)
        when REMOTE_ASYNC
          if (instance = @instances[msg[2]])
            begin
              instance.__send__(msg.fetch(3), *msg.fetch(4), &msg[5])
            rescue LibZMQ::ETERMError => e
              raise e
            rescue => e
              ZMQ.logger.crash(e)
            end
          end
        end
      rescue => e
        exec_msg = [EXCEPTION, e].to_msgpack
        LibZMQ.msg_send(peer, @remote_server, LibZMQ::SNDMORE)
        LibZMQ.send(@remote_server, exec_msg, 0)
      end
    rescue => e
      ZMQ.logger.crash(e)
    end
  end
end

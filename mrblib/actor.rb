unless ZMQ.const_defined?("Poller")
  raise RuntimeError, "mruby-actor needs libzmq with poller support"
end

class Actor < ZMQ::Thread
  def self.new(options = {}) # this starts the background thread
    super(Actor_fn, options)
  end

  def remote_new(peerid, mrb_class, *args)
    if block_given?
      raise ArgumentError, "blocks cannot be migrated"
    end
    LibZMQ.send(@pipe, {type: :remote_new, peerid: peerid, class: mrb_class, args: args}.to_msgpack, 0)
    msg = MessagePack.unpack(@pipe.recv.to_str(true))
    case msg[:type]
    when :instance
      RemoteThreadProxy.new(self, peerid, msg[:object_id])
    when :exception
      raise msg[:exception]
    end
  end

  def remote_send(peerid, object_id, method, *args)
    LibZMQ.send(@pipe, {type: :remote_send, peerid: peerid, object_id: object_id, method: method, args: args}.to_msgpack, 0)
    msg = MessagePack.unpack(@pipe.recv.to_str(true))
    case msg[:type]
    when :result
      msg[:result]
    when :exception
      raise msg[:exception]
    end
  end

  def remote_async(peerid, object_id, method, *args)
    LibZMQ.send(@pipe, {type: :remote_async, peerid: peerid, object_id: object_id, method: method, args: args}.to_msgpack, 0)
  end

  def remote_actors
    LibZMQ.send(@pipe, {type: :remote_actors}.to_msgpack, 0)
    msg = MessagePack.unpack(@pipe.recv.to_str(true))
    case msg[:type]
    when :actors
      msg[:actors]
    when :exception
      raise msg[:exception]
    end
  end

  class RemoteThreadProxy
    attr_reader :object_id

    def initialize(thread, peerid, object_id)
      @thread = thread
      @peerid = peerid
      @object_id = object_id
    end

    def send(m, *args)
      if block_given?
        raise ArgumentError, "blocks cannot be migrated"
      end
      @thread.remote_send(@peerid, @object_id, m, *args)
    end

    def async(m, *args)
      if block_given?
        raise ArgumentError, "blocks cannot be migrated"
      end
      @thread.remote_async(@peerid, @object_id, m, *args)
      nil
    end

    def respond_to?(m)
      super(m) || @thread.remote_send(@peerid, @object_id, :respond_to?, m)
    end

    def method_missing(m, *args)
      if block_given?
        raise ArgumentError, "blocks cannot be migrated"
      end
      @thread.remote_send(@peerid, @object_id, m, *args)
    end
  end

  class Actor_fn < ZMQ::Thread_fn
    ROUTER_ENDPOINT = 'router_endpoint'.freeze
    ROUTER_PUBLIC_KEY = 'router_public_key'.freeze
    MRB_ACTOR_V2 = 'mrb-actor-v2'.freeze

    def initialize(options) # these are the options passed to the frontend Actor constructor, they are saved as @options
      super
      unless @auth
        @auth = ZMQ::Zap.new(authenticator: ZMQ::Zap::Authenticator.new)
        @poller << @auth
      end
      server_keypair = @options.fetch(:server_keypair) { LibZMQ.curve_keypair }
      @client_keypair = @options.fetch(:client_keypair) { LibZMQ.curve_keypair }
      @remote_server = ZMQ::Router.new
      @poller << @remote_server
      @remote_server.curve_security(type: :server, secret_key: server_keypair[:secret_key])
      unless @remote_server.mechanism == LibZMQ::CURVE
        raise RuntimeError, "cannot set curve security"
      end
      @remote_server.bind(@options.fetch(:remote_server_endpoint))
      @remote_server.identity = @remote_server.last_endpoint
      @zyre = Zyre.new
      @poller << @zyre
      @zyre[ROUTER_ENDPOINT] = @remote_server.last_endpoint
      @zyre[ROUTER_PUBLIC_KEY] = server_keypair[:public_key]
      @zyre.join(MRB_ACTOR_V2)
      @remote_dealers = {}
      @zyre.start
    end

    def run
      until @interrupted
        @poller.wait do |socket, events|
          case socket
          when @pipe
            handle_pipe
          when @auth
            @auth.handle_zap
          when @zyre
            handle_zyre
          when @remote_server
            handle_server
          end
        end
      end
    end

    def handle_pipe
      msg = @pipe.recv.to_str(true)
      if msg == TERM
        @interrupted = true
      else
        msg = MessagePack.unpack(msg)
        begin
          case msg[:type]
          when :new
            instance = msg[:class].new(*msg[:args])
            id = instance.__id__
            @instances[id] = instance
            LibZMQ.send(@pipe, {type: :instance, object_id: id}.to_msgpack, 0)
          when :send
            LibZMQ.send(@pipe, {type: :result, result: @instances.fetch(msg[:object_id]).__send__(msg[:method], *msg[:args])}.to_msgpack, 0)
          when :async
            if (instance = @instances[msg[:object_id]])
              begin
                instance.__send__(msg[:method], *msg[:args])
              rescue => e
                ZMQ.logger.crash(e)
              end
            end
          when :finalize
            @instances.delete(msg[:object_id])
          when :remote_new, :remote_send
            peerid = msg.delete(:peerid)
            dealer = @remote_dealers.fetch(peerid) do
              dealer = ZMQ::Dealer.new
              dealer.curve_security(type: :client, server_key: @zyre.peer_header_value(peerid, ROUTER_PUBLIC_KEY),
                public_key: @client_keypair[:public_key], secret_key: @client_keypair[:secret_key])
              unless dealer.mechanism == LibZMQ::CURVE
                raise RuntimeError, "cannot set curve security"
              end
              dealer.rcvtimeo = 120000
              dealer.sndtimeo = 120000
              dealer.connect(@zyre.peer_header_value(peerid, ROUTER_ENDPOINT))
              @remote_dealers[peerid] = dealer
              dealer
            end
            LibZMQ.send(dealer, msg.to_msgpack, 0)
            LibZMQ.msg_send(dealer.recv, @pipe, 0)
          when :remote_async
            peerid = msg.delete(:peerid)
            dealer = @remote_dealers.fetch(peerid) do
              dealer = ZMQ::Dealer.new
              dealer.curve_security(type: :client, server_key: @zyre.peer_header_value(peerid, ROUTER_PUBLIC_KEY),
                public_key: @client_keypair[:public_key], secret_key: @client_keypair[:secret_key])
              unless dealer.mechanism == LibZMQ::CURVE
                raise RuntimeError, "cannot set curve security"
              end
              dealer.rcvtimeo = 120000
              dealer.sndtimeo = 120000
              dealer.connect(@zyre.peer_header_value(peerid, ROUTER_ENDPOINT))
              @remote_dealers[peerid] = dealer
              dealer
            end
            LibZMQ.send(dealer, msg.to_msgpack, 0)
          when :remote_actors
            actors = []
            @zyre.peers_by_group(MRB_ACTOR_V2).each do |peerid|
              actors << {
                peerid: peerid,
                router_endpoint: @zyre.peer_header_value(peerid, ROUTER_ENDPOINT),
                router_public_key: @zyre.peer_header_value(peerid, ROUTER_PUBLIC_KEY)
              }
            end
            LibZMQ.send(@pipe, {type: :actors, actors: actors}.to_msgpack, 0)
          end
        rescue => e
          LibZMQ.send(@pipe, {type: :exception, exception: e}.to_msgpack, 0)
        end
      end
    rescue => e
      ZMQ.logger.crash(e)
    end

    EXIT = 'EXIT'.freeze

    def handle_zyre
      event, peerid, *_ = @zyre.recv
      if event == EXIT
        @remote_dealers.delete(peerid)
      end
    rescue => e
      ZMQ.logger.crash(e)
    end

    def handle_server
      peer, msg = @remote_server.recv
      msg = MessagePack.unpack(msg.to_str(true))
      begin
        case msg.fetch(:type)
        when :remote_new
          instance = msg.fetch(:class).new(*msg.fetch(:args))
          id = instance.__id__
          instance_msg = {
            type: :instance,
            object_id: id
          }.to_msgpack
          @instances[id] = instance
          LibZMQ.msg_send(peer, @remote_server, LibZMQ::SNDMORE)
          LibZMQ.send(@remote_server, instance_msg, 0)
        when :remote_send
          result = {
            type: :result,
            result: @instances.fetch(msg.fetch(:object_id)).__send__(msg.fetch(:method), *msg.fetch(:args))
          }.to_msgpack
          LibZMQ.msg_send(peer, @remote_server, LibZMQ::SNDMORE)
          LibZMQ.send(@remote_server, result, 0)
        when :remote_async
          if (instance = @instances[msg[:object_id]])
            begin
              instance.__send__(msg.fetch(:method), *msg.fetch(:args))
            rescue => e
              ZMQ.logger.crash(e)
            end
          end
        end
      rescue => e
        exec_msg = {
          type: :exception,
          exception: e
        }.to_msgpack
        LibZMQ.msg_send(peer, @remote_server, LibZMQ::SNDMORE)
        LibZMQ.send(@remote_server, exec_msg, 0)
      end
    rescue => e
      ZMQ.logger.crash(e)
    end
  end
end

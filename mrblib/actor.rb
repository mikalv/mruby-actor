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

  def remote_peers
    LibZMQ.send(@pipe, {type: :remote_peers}.to_msgpack, 0)
    msg = MessagePack.unpack(@pipe.recv.to_str(true))
    case msg[:type]
    when :peers
      msg[:peers]
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
    def initialize(options) # these are the options passed to the frontend Actor constructor, they are saved as @options
      super
      @client_keypair = @options.fetch(:client_keypair) { LibZMQ.curve_keypair }
      @remote_server = ZMQ::Router.new(@options.fetch(:remote_server_endpoint))
      @poller << @remote_server
      keypair = @options.fetch(:server_keypair) { LibZMQ.curve_keypair }
      @remote_server.curve_security(type: :server, public_key: keypair[:public_key], secret_key: keypair[:secret_key])
      unless @auth
        @auth = ZMQ::Zap.new(authenticator: ZMQ::Zap::Authenticator.new)
        @poller << @auth
      end
      @zyre = Zyre.new(@options[:zyre])
      @poller << @zyre
      @zyre["router_endpoint"] = @remote_server.last_endpoint
      @zyre["router_public_key"] = keypair[:public_key]
      @zyre.join("mrb-actor-v2")
      @remote_dealers = {}
      @remote_actors = {}
    end

    def run
      @zyre.start
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
              dealer = ZMQ::Dealer.new(@zyre.peer_header_value(peerid, "router_endpoint"))
              dealer.curve_security(type: :client, server_key: @zyre.peer_header_value(peerid, "router_public_key"),
                public_key: @client_keypair[:public_key], secret_key: @client_keypair[:secret_key])
              @remote_dealers[peerid] = dealer
              dealer
            end
            LibZMQ.send(dealer, msg.to_msgpack, 0)
            LibZMQ.msg_send(dealer.recv, @pipe, 0)
          when :remote_async
            peerid = msg.delete(:peerid)
            dealer = @remote_dealers.fetch(peerid) do
              dealer = ZMQ::Dealer.new(@zyre.peer_header_value(peerid, "router_endpoint"))
              dealer.curve_security(type: :client, server_key: @zyre.peer_header_value(peerid, "router_public_key"),
                public_key: @client_keypair[:public_key], secret_key: @client_keypair[:secret_key])
              @remote_dealers[peerid] = dealer
              dealer
            end
            LibZMQ.send(dealer, msg.to_msgpack, 0)
          when :remote_peers
            peers = {}
            @zyre.peers_by_group("mrb-actor-v2").each do |peerid|
              peers[peerid] = {
                router_endpoint: @zyre.peer_header_value(peerid, "router_endpoint"),
                router_public_key: @zyre.peer_header_value(peerid, "router_public_key")
              }
            end
            LibZMQ.send(@pipe, {type: :peers, peers: peers}.to_msgpack, 0)
          end
        rescue => e
          LibZMQ.send(@pipe, {type: :exception, exception: e}.to_msgpack, 0)
        end
      end
    rescue => e
      ZMQ.logger.crash(e)
    end

    def handle_zyre
      event, peerid, *_ = @zyre.recv
      if event == 'EXIT'
        @remote_dealers.delete(peerid)
      end
    rescue => e
      ZMQ.logger.crash(e)
    end

    def handle_server
      peer, msg = @remote_server.recv
      msg = MessagePack.unpack(msg.to_str(true))
      begin
        case msg[:type]
        when :remote_new
          instance = msg[:class].new(*msg[:args])
          id = instance.__id__
          @instances[id] = instance
          LibZMQ.msg_send(peer, @remote_server, LibZMQ::SNDMORE)
          LibZMQ.send(@remote_server, {type: :instance, object_id: id}.to_msgpack, 0)
        when :remote_send
          LibZMQ.msg_send(peer, @remote_server, LibZMQ::SNDMORE)
          LibZMQ.send(@remote_server, {type: :result, result: @instances.fetch(msg[:object_id]).__send__(msg[:method], *msg[:args])}.to_msgpack, 0)
        when :remote_async
          if (instance = @instances[msg[:object_id]])
            begin
              instance.__send__(msg[:method], *msg[:args])
            rescue => e
              ZMQ.logger.crash(e)
            end
          end
        end
      rescue => e
        LibZMQ.msg_send(peer, @remote_server, LibZMQ::SNDMORE)
        LibZMQ.send(@remote_server, {type: :exception, exception: e}.to_msgpack, 0)
      end
    rescue => e
      ZMQ.logger.crash(e)
    end
  end
end

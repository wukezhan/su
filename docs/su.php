<?php
namespace su {
    /**
     * Class coroutine
     * @package su
     * @alias co
     */
    class coroutine
    {
        /**
         * Runs a callable as coroutine.
         * @param Callable $callable The callable to be run.
         * @return NULL
         */
        public static function run($callable)
        {
            return NULL;
        }
        /**
         * Yields the processor, allowing other coroutines to run. It does not
         * suspend the current coroutine, so execution resumes automatically.
         */
        public static function sched()
        {
            return NULL;
        }
        /**
         * Delays the coroutine execution for the given number of milliseconds,
         * it blocks the coroutine, but does not block the process.
         * @param int $time Delays the coroutine execution for the given number of milliseconds.
         * @return NULL
         */
        public static function sleep($time)
        {
            return NULL;
        }
    }

    class co extends coroutine {};

    /**
     * Class channel.
     * Channels are a typed conduit through which you can send and
     * receive data with the channel method send & recv
     * @package su
     * @alias chan
     */
    class channel
    {

        /**
         * Constructs a channel
         * @param int $cap The capacity of the channel buffer
         * @return channel
         */
        public function __construct($cap=2)
        {
            //
        }

        /**
         * Sends data to a coroutine, it can only be called in coroutines.
         * Any coroutine who calls the send method, called the sender coroutine.
         * if the channel buffer is full, it yields to the coroutines scheduler
         * and blocks the sender until some receiver has retrieved a value.
         * Otherwise, it copies the data to channel buffer directly.
         * @param mixed $data The value to be sent to the receiver coroutine.
         */
        public function send($data)
        {
            //
        }

        /**
         * Receives data from channel, it can only be called in coroutines.
         * Any coroutine who calls the recv method, called the receiver coroutine.
         * if no data in channel, it yields to coroutines scheduler and blocks the
         * receiver until some receiver has sent a value.
         * Otherwise, it returns data from channel buffers directly.
         * 
         * NOTE: if the channel is timeout or been cleaned, it will return NULL(or throw exception or return su\chan\error?)
         * @return mixed
         */
        public function recv()
        {
            //
        }
    }

    /**
     * Class process.
     * Manages the running process and spawns child processes.
     * @package su
     */
    class process
    {
        /**
         * Returns the running process instance.
         * @return process
         */
        public static function running()
        {
            return new process();
        }

        public function __construct()
        {
            //
        }

        /**
         * Daemonizes the master process, it can not work on the worker processes.
         * returns true if it's called in master.
         * @return bool
         */
        public function daemonize()
        {
            return true;
        }

        /**
         * Finds whether a process is a master.
         * @return bool
         */
        public function is_master()
        {
            return true;
        }

        /**
         * Finds whether a process is a worker.
         * @return bool
         */
        public function is_worker()
        {
            return true;
        }

        /**
         * Adds $callable as the listener coroutine for the event $event.
         * @param mixed $event
         * @param callable $callable
         * @return $this
         */
        public function on($event, $callable)
        {
            return $this;
        }

        /**
         * Runs process.
         */
        public function run()
        {
            //
        }

        /**
         * Sends message to process.
         * @param string $msg The message to be sent
         * @param null $handle The handle to be sent
         */
        public function send($msg, $handle=null)
        {
            //
        }

        /**
         * Sets the process options of the process.
         * @param array $options The options to be set to process.
         * @return $this
         */
        public function set_options($options)
        {
            return $this;
        }

        /**
         * Sets the process title of the process.
         * @param string $title The title to be set.
         * @return $this
         */
        public function set_title($title)
        {
            return $this;
        }

        /**
         * Spawns child process
         * @param string $cmd <p>
         * test
         * </p>
         * @return bool
         */
        public static function spawn(string $cmd, $args, $options): bool
        {
            //
        }
    }

    /**
     * Class timer.
     * High resolution timer
     */
    class timer
    {
        public function __construct()
        {
            //
        }

        /**
         * Calls $callable after $ms ms
         * @param int $ms 
         */
        public function after(int $ms, callable $callable)
        {
            //
        }

        /**
         * Calls $callable every $ms ms
         */
        public function every(int $ms, callable $callable)
        {
            //
        }

        public function again()
        {
            //
        }

        public function close()
        {
            //
        }
    }
}
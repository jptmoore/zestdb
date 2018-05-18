type t;

let create : path_to_db::string => max_buffer_size::int => shard_size::int => t;
let flush : ctx::t => info::string => Lwt.t unit;
let write : ctx::t => info::string => timestamp::option int ? => id::string => json::Ezjsonm.t => Lwt.t unit;
let read_last : ctx::t => id_list::list string => n::int => Lwt.t Ezjsonm.t;
let read_latest : ctx::t => id_list::list string => Lwt.t Ezjsonm.t;
let read_first : ctx::t => id_list::list string => n::int => Lwt.t Ezjsonm.t;
let read_since : ctx::t => id_list::list string => from::int => Lwt.t Ezjsonm.t;
let read_range : ctx::t => id_list::list string => from::int => to::int => Lwt.t Ezjsonm.t;
let read_earliest : ctx::t => id_list::list string => Lwt.t Ezjsonm.t;
let length : ctx::t => id_list::list string => Lwt.t Ezjsonm.t;
let delete : ctx::t => info::string => id_list::list string => json::Lwt.t Ezjsonm.t => Lwt.t unit;
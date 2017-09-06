open Lwt.Infix;

let req_endpoint = ref "tcp://127.0.0.1:5555";
let sub_endpoint = ref "tcp://127.0.0.1:5556";
let notify_list  = ref [];

let kv_json_store = ref (Database.Json.Kv.create file::"./kv-json-store");
let ts_json_store = ref (Database.Json.Ts.create file::"./ts-json-store");

let publish path payload socket => {
  let msg = Printf.sprintf "%s %s" path payload;
  Lwt_zmq.Socket.send socket msg;
};

let handle_header bits => {
  let tuple = [%bitstring
    switch bits {
    | {|tkl : 4 : unsigned;
        oc : 4 : unsigned; 
        code : 8 : unsigned; 
        rest : -1 : bitstring
     |} => (tkl, oc, code, rest); 
    | {|_|} => failwith "invalid header";
    };
  ];
  tuple;    
};

let handle_token bits len => {
  let tuple = [%bitstring
    switch bits {
    | {|token : len*8 : string; 
        rest : -1 : bitstring
      |} => (token, rest);
    | {|_|} => failwith "invalid token";
    };
  ];
  tuple;
};

let handle_option bits => {
  let tuple = [%bitstring
    switch bits {
    | {|number : 4 : unsigned; 
        len : 16 : bigendian;
        0xf : 4 : unsigned; 
        value: len*8: string; 
        rest : -1 : bitstring
      |} => (number, value, rest);
    | {|_|} => failwith "invalid options";
    };
  ];
  tuple;
};

let handle_options oc bits => {
  let options = Array.make oc (0,"");
  let rec handle oc bits =>
    if (oc == 0) {
      bits;
    } else {
      let (number, value, r) = handle_option bits;
      Array.set options (oc - 1) (number,value);
      let _ = Lwt_io.printf "option => %d:%s\n" number value;
      handle (oc - 1) r
  };
  (options, handle oc bits);
};

let create_header tkl::tkl oc::oc code::code => {
  let bits = [%bitstring 
    {|tkl : 4 : unsigned;
      oc : 4 : unsigned;
      code : 8 : unsigned
    |}
  ];
  (bits, 16);
};

let create_option number::number value::value => {
  let byte_length = String.length value;
  let bit_length = byte_length * 8;
  let bits = [%bitstring 
    {|number : 4 : unsigned;
      byte_length : 16 : bigendian;
      0xf : 4 : unsigned;
      value : bit_length : string
    |}
  ];
  (bits ,(bit_length+24));
};

let ack_created () => {
  let (header_value, header_length) = create_header tkl::0 oc::0 code::64;
  let bits = [%bitstring 
    {|header_value : header_length : bitstring|}
  ];
  Bitstring.string_of_bitstring bits;
};

let ack_content payload => {
  let (header_value, header_length) = create_header tkl::0 oc::1 code::69;
  let (format_value, format_length) = create_option number::12 value::"application/json";
  let payload_bytes = String.length payload * 8;
  let bits = [%bitstring 
    {|header_value : header_length : bitstring;
      format_value : format_length : bitstring;
      payload : payload_bytes : string
    |}
  ];
  Bitstring.string_of_bitstring bits;
};

let get_option_value options value => {
  let rec find a x i => {
    let (number,value) = a.(i);
    if (number == x) {
      value;
    } else {
      find a x (i + 1)
    };
  };
  find options value 0;
};

let get_content_format options => {
  let value = get_option_value options 12;
  let bits = Bitstring.bitstring_of_string value;
  let id = [%bitstring
    switch bits {
    | {|id : 8 : unsigned|} => id;
    | {|_|} => failwith "invalid content value";
    };
  ];
  id;
};

let has_observed options => {
  if (Array.exists (fun (number,_) => number == 6) options) {
    true;
  } else {
    false;
  }
};

let is_observed path => {
  List.mem path !notify_list;
};

let add_to_observe path => {
  if (is_observed path == false) {
    let _ = Lwt_io.printf "adding %s to notify list\n" path;
    notify_list := List.cons path !notify_list;
  };
};

let get_key_mode uri_path => {
  let key = Str.string_after uri_path 4;
  let mode = Str.first_chars uri_path 4;
  (key,mode);
};

let handle_get_read_ts_latest path_list => {
  let id = List.nth path_list 2;
  Database.Json.Ts.read_latest !ts_json_store id;
};

let handle_get_read_ts_last path_list => {
  let id = List.nth path_list 2;
  let n = List.nth path_list 4;
  Database.Json.Ts.read_last !ts_json_store id (int_of_string n);
};

let handle_get_read_ts_since path_list => {
  let id = List.nth path_list 2;
  let t = List.nth path_list 4;
  Database.Json.Ts.read_since !ts_json_store id (int_of_string t);
};

let handle_get_read_ts_range path_list => {
  let id = List.nth path_list 2;
  let t1 = List.nth path_list 4;
  let t2 = List.nth path_list 5;
  Database.Json.Ts.read_range !ts_json_store id (int_of_string t1) (int_of_string t2);
};

let handle_get_read_ts uri_path => {
  let path_list = String.split_on_char '/' uri_path;
  let mode = List.nth path_list 3;
  switch mode {
  | "latest" => handle_get_read_ts_latest path_list;
  | "last" => handle_get_read_ts_last path_list;
  | "since" => handle_get_read_ts_since path_list;
  | "range" => handle_get_read_ts_range path_list;
  | _ => failwith ("unsupported get ts mode:" ^ mode);
  };
};

let handle_get_read uri_path => {
  let (key,mode) = get_key_mode uri_path;
  switch mode {
  | "/kv/" => Database.Json.Kv.read !kv_json_store key;
  | "/ts/" => handle_get_read_ts uri_path;
  | _ => failwith "unsupported get mode";
  }
};

let handle_post_write uri_path payload => {
  let (key,mode) = get_key_mode uri_path;
  switch mode {
  | "/kv/" => Database.Json.Kv.write !kv_json_store key (Ezjsonm.from_string payload);
  | "/ts/" => Database.Json.Ts.write !ts_json_store key (Ezjsonm.from_string payload);
  | _ => failwith "unsupported post mode";
  }
};

let handle_get options => {
  let uri_path = get_option_value options 11;
  if (has_observed options) {
    add_to_observe uri_path;
    ack_created () |> Lwt.return;
  } else {
    handle_get_read uri_path >>=
      fun resp => ack_content (Ezjsonm.to_string resp) |> Lwt.return;
  };
};

let assert_content_format options => {
  let content_format = get_content_format options;
  let _ = Lwt_io.printf "content_format => %d\n" content_format;
  assert (content_format == 50);
};

let handle_post options payload with::pub_soc => {
  /* we are just accepting json for now */
  assert_content_format options;
  let uri_path = get_option_value options 11;
  if (is_observed uri_path) {
    publish uri_path payload pub_soc >>=
      fun () => ack_created () |> Lwt.return;
  } else {
    handle_post_write uri_path payload >>=
      fun () => ack_created () |> Lwt.return;
  };
};

let handle_msg msg with::pub_soc => {
  Lwt_io.printlf "Received: %s" msg >>=
    fun () => {
      let r0 = Bitstring.bitstring_of_string msg;
      let (tkl, oc, code, r1) = handle_header r0;
      let (token, r2) = handle_token r1 tkl;
      let (options,r3) = handle_options oc r2;
      let payload = Bitstring.string_of_bitstring r3;
      switch code {
      | 1 => handle_get options;
      | 2 => handle_post options payload with::pub_soc;
      | _ => failwith "invalid code";
      };
    };  
};

let server with::rep_soc and::pub_soc => {
  let rec loop () => {
    Lwt_zmq.Socket.recv rep_soc >>=
      fun msg =>
        handle_msg msg with::pub_soc >>=
          fun resp =>
            Lwt_zmq.Socket.send rep_soc resp >>=
              fun () => 
                Lwt_io.printf "Sending: %s\n" resp >>=
                  fun () => loop ();
  };
  loop ();
};

let connect_socket endpoint ctx kind secret => {
  let soc = ZMQ.Socket.create ctx kind; 
  ZMQ.Socket.set_curve_server soc true;
  ZMQ.Socket.set_curve_secretkey soc secret; 
  ZMQ.Socket.bind soc endpoint;
  Lwt_zmq.Socket.of_socket soc;
};

let close_socket lwt_soc => {
  let soc = Lwt_zmq.Socket.to_socket lwt_soc;
  ZMQ.Socket.close soc;
};

let debug_mode = ref false;
let curve_secret_key = ref "";

/* test key: uf4XGHI7[fLoe&aG1tU83[ptpezyQMVIHh)J=zB1 */


let () = {
  let usage = "usage: " ^ Sys.argv.(0) ^ " [--debug] [--secret-key string]";
  let speclist = [
    ("--request-endpoint", Arg.Set_string req_endpoint, ": to set the request/reply endpoint"),
    ("--subscribe-endpoint", Arg.Set_string sub_endpoint, ": to set the subscribe endpoint"),
    ("--debug", Arg.Set debug_mode, ": turn debug mode on"),
    ("--secret-key", Arg.Set_string curve_secret_key, ": to set the curve secret key"),
  ];
  Arg.parse speclist (fun x => raise (Arg.Bad ("Bad argument : " ^ x))) usage;
  let _ = Lwt_io.printf "Running server...\n";
  let ctx = ZMQ.Context.create ();
  let rep_soc = connect_socket !req_endpoint ctx ZMQ.Socket.rep !curve_secret_key;
  let pub_soc = connect_socket !sub_endpoint ctx ZMQ.Socket.publ !curve_secret_key;
  let () = Lwt_main.run { server with::rep_soc and::pub_soc};
  /* we never get here */
  close_socket pub_soc;
  close_socket rep_soc;
  ZMQ.Context.terminate ctx
};
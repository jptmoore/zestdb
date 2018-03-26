open Lwt.Infix;

module Store = Ezirmin.FS_lww_register (Tc.List(Tc.String));

type t = Store.branch;

let create ::file => {
  Store.init root::file bare::true () >>= Store.master;
};

let write branch k v => {
  branch >>= fun branch' =>
  Store.write message::k branch' path::[k] v;
};
  
let read branch k => {
  branch >>= fun branch' =>
    Store.read branch' path::[k];
};

let add v lis => {
  open List;
  if (exists (fun x => x == v) lis) {
    None;
  } else {
    Some (cons v lis);
  }
};

let get branch id => {
  open Ezjsonm;
  read branch id >>= fun data =>
    switch data {
    | Some lis => strings lis;
    | None => `A [];
    } |> Lwt.return;
};

let update branch id v => {
  read branch id >>= fun data =>
    switch data {
    | Some curr_lis => {
        switch (add v curr_lis) {
        | Some new_lis => write branch id new_lis; 
        | None => Lwt.return_unit;
        }
      };
    | None => write branch id [v];
    };
};
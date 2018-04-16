open Lwt.Infix;


type t = {
  q: Queue.t (int, Ezjsonm.t),
  mutable disk_range: option (int, int),
  mutable ascending_series: bool,
  mutable descending_series: bool
};

let create () => {
  {
    q: Queue.create (),
    disk_range: None,
    ascending_series: true,
    descending_series: true
  };
};

let push ctx n => {
  Queue.push n ctx.q;
};

let pop ctx => {
  Queue.pop ctx.q;
};

let length ctx => {
  Queue.length ctx.q;
};

let to_list ctx => {
  Queue.fold (fun x y => List.cons y x) [] ctx.q;
};


let is_ascending ctx ub => {
  let rec is_sorted lis => {
    switch lis {
    | [(t1,j1), (t2,j2), ...l] => t1 >= t2 && is_sorted [(t2,j2), ...l]
    | _ => true;
    };
  };
  switch (to_list ctx) {
  | [] => true
  | [(t,j), ...l] => is_sorted [(t,j), ...l] && t >= ub;
  };
};

let is_descending ctx lb => {
  let rec is_sorted lis => {
    switch lis {
    | [(t1,j1), (t2,j2), ...l] => t1 <= t2 && is_sorted [(t2,j2), ...l]
    | _ => true;
    };
  };
  switch (to_list ctx) {
  | [] => true
  | [(t,j), ...l] => is_sorted [(t,j), ...l] && t <= lb;
  };
};

let clear ctx => {
  Queue.clear ctx.q;
};

let set_disk_range ctx range => {
  ctx.disk_range = range;
};

let get_disk_range ctx => {
  ctx.disk_range;
};

let set_ascending_series ctx v => {
  ctx.ascending_series = v;
};

let get_ascending_series ctx => {
  ctx.ascending_series;
};

let set_descending_series ctx v => {
  ctx.descending_series = v;
};

let get_descending_series ctx => {
  ctx.descending_series;
};
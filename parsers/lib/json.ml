type number =
  | Int of int
  | Float of float
[@@deriving show]

type t =
  | Number of number
  | String of string
  | Bool of bool
  | Array of t list
  | Object of (string * t) list
  | Null
[@@deriving show]

module Memo = struct
  type ('k, 'v) t = ('k, 'v) Hashtbl.t

  let init () = Hashtbl.create 3
  let of_list l = Hashtbl.of_seq @@ List.to_seq l

  let check memo key cond =
    match Hashtbl.find_opt memo key with
    | None -> false
    | Some v -> cond v
  ;;

  let load memo key = Hashtbl.find memo key

  let store memo key value =
    Hashtbl.add memo key value;
    memo
  ;;

  let update memo key fn =
    match Hashtbl.find_opt memo key with
    | None -> memo
    | Some v ->
      Hashtbl.replace memo key @@ fn v;
      memo
  ;;
end

module State = struct
  type t =
    { str : string
    ; len : int
    ; pos : int
    }

  let init str = { str; len = String.length str; pos = 0 }
  let next state = { state with pos = state.pos + 1 }

  let curr state =
    try
      let c = String.get state.str state.pos in
      Some c
    with
    | Invalid_argument _ -> None
  ;;

  let prev state =
    try
      let c = String.get state.str (state.pos - 1) in
      Some c
    with
    | Invalid_argument _ -> None
  ;;

  let accept state cond =
    let rec go state =
      match curr state with
      | Some c when cond c -> go @@ next state
      | _ -> state
    in
    go state
  ;;

  let accept_memo (type k v)
    : t -> ?memo:(k, v) Memo.t -> ((k, v) Memo.t -> char -> (k, v) Memo.t * bool) -> t
    =
    fun state ?(memo = Memo.init ()) cond ->
    let rec go memo state =
      match curr state with
      | None -> state
      | Some c ->
        let memo, accepted = cond memo c in
        if not accepted then state else go memo @@ next state
    in
    go memo state
  ;;
end

module Accept = struct
  let whitespace state =
    State.accept state
    @@ function
    | ' ' | '\t' | '\r' | '\n' -> true
    | _ -> false
  ;;

  (* todo: scientific notation *)
  let number state =
    let memo = Memo.of_list [ '.', 0 ] in
    State.accept_memo ~memo state
    @@ fun memo -> function
    | c when c >= '0' && c <= '9' -> memo, true
    | '.' ->
      if Memo.check memo '.' (fun x -> x = 0)
      then Memo.update memo '.' (fun x -> x + 1), true
      else memo, false
    | _ -> memo, false
  ;;

  let string state =
    let memo = Memo.of_list [ '"', 0 ] in
    let quote () =
      match State.prev state with
      | Some '\\' -> memo
      | _ -> Memo.update memo '"' (fun x -> x + 1)
    in
    State.accept_memo ~memo state
    @@ fun memo -> function
    | '"' ->
      (match Memo.load memo '"' with
       | 0 | 1 -> quote (), true
       | _ -> memo, false)
    | _ ->
      (match Memo.load memo '"' with
       | 0 | 2 -> memo, false
       | _ -> memo, true)
  ;;
end

let parsers = Accept.[ string; number; whitespace ]

let is_valid str =
  let open State in
  let apply state parser =
    match parser state with
    | next when next.pos = next.len || next.pos > state.pos -> Some next
    | _ -> None
  in
  let rec go state =
    if state.pos = state.len
    then true
    else (
      let apply = apply state in
      match List.find_map apply parsers with
      | Some next -> go next
      | None -> false)
  in
  go (init str)
;;

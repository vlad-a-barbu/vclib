open Effect
open Effect.Deep

type _ Effect.t += Fork : (unit -> unit) -> unit Effect.t | Yield : unit Effect.t

let fork fn = perform @@ Fork fn
let yield () = perform Yield

let run prog =
  let q = Queue.create () in
  let enqueue k = Queue.push (fun () -> continue k ()) q in
  let dequeue () = if Queue.is_empty q then () else Queue.pop q () in
  let rec step fn =
    match fn () with
    | () -> dequeue ()
    | exception exn ->
      Printexc.to_string exn |> print_endline;
      dequeue ()
    | effect Fork fn, k ->
      enqueue k;
      step fn
    | effect Yield, k ->
      enqueue k;
      dequeue ()
  in
  step prog
;;

# require[_relative] doesn't like non .rb/.so/.a extensions in MRI
load File.expand_path('./observable.mrb', __dir__)

empty = Observable.create { |observer| observer.complete }
value = -> (v) { Observable.create { |observer| observer.next(v) } }
failure = Observable.create { |observer| observer.error("some error") }

subscriptions = {
  next: -> (x) { puts "next: '#{x}'" },
  error: -> (x) { puts "error: '#{x}'" },
  complete: -> () { puts "complete" }
}

empty.subscribe(subscriptions)
value.("hello world").subscribe(subscriptions)
value.("hello world").subscribe { |x| puts "block based subscription: '#{x}'" }
value.(2).map { |x| x + 1 } .subscribe(subscriptions)
failure.subscribe(subscriptions)
failure # prints nothing, observables are lazy!

# output will be:

# complete
# next: 'hello world'
# block based sub hello world
# next: '3'
# error: 'some error'

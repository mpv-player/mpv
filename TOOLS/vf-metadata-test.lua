mp.command('vf add @autocrop.cropdetect:lavfi=graph="cropdetect=limit=24:round=2:reset=0"')

function print_tree(prefix,tree)
   if(tree)then
      if(type(tree)=="table")then
	 io.write("{\n")
	 for key,value in pairs(tree)do
	    io.write(prefix,key," = ")
	    print_tree(prefix.."\t",value)
	 end
	 io.write(prefix,"}\n")
      else
	 io.write(tree,"\n")
      end
   else
      io.write("nil\n")
   end
end

function print_test(key,fun)
   io.write(key," = ")
   ret=fun(key)
   if (ret) then
      if (type(ret)=="table") then
	 print_tree("\t",ret)
      else
	 io.write(ret,"\n")
      end
   else
      io.write('(nil)\n')
   end
   io.write("\n")
   return ret
end

function test_node(path,fun)
   print_test(path,fun)
   print_test(path.."/list",fun)
   num=print_test(path.."/list/count",fun)
   if(num) then
      for i=0,num-1 do
	 list_item=string.format("/list/%d",i)
	 print_test(path..list_item,fun)
	 print_test(path..list_item.."/key",fun)
	 print_test(path..list_item.."/value",fun)
      end
   end
end

function test_with_get_fun(fun)
   test_node("metadata",fun)
   test_node("vf-metadata",fun)
   test_node("vf-metadata/by-label/autocrop.cropdetect",fun)
   print('what')
   print_test("vf-metadata/lavfi",fun)
   print('where')
   test_node("vf-metadata/lavfi",fun)
   print_test("vf-metadata/lavfi/lavfi.cropdetect.x",fun)
   io.write("*****errors conditions*****\n")
   print_test("vf-metadata/list/count=",fun)
   print_test("vf-metadata/list/nonsense",fun)
   print_test("vf-metadata/list/0/nonsense",fun)
   print_test("vf-metadata/list/0/key/nonsense",fun)
   print_test("vf-metadata/list/0/value/nonsense",fun)
   print_test("vf-metadata/list/count/nonsense",fun)
   print_test("vf-metadata/by-key/nonsense",fun)

   print_test("vf-metadata/nonsense",fun)
   print_test("vf-metadata/lavfi/nonsense",fun)
   print_test("vf-metadata/lavfi/cropdetect/nonsense",fun)
   print_test("vf-metadata/lavfi/cropdetect/x2/nonsense",fun)
end

function test()
   io.write("**********get property native**********\n")
   test_with_get_fun(mp.get_property_native)
   io.write("**********get property**********\n")
   test_with_get_fun(mp.get_property)
   mp.command("quit")
end

function wait_and_test()
   mp.add_timeout(1,test)
end

mp.register_event("playback-restart",wait_and_test)

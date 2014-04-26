script_name=mp.get_script_name():gsub("lua/","",1)

function print_state(params)
   if params then
      mp.osd_message(script_name..':\n'
			.."method = "..params['method'].."\n"
			.."target = "..params['target'])
   else
      mp.osd_message(script_name..':\noff')
   end
end

function get_index_of_drc(afs)
   for i,af in pairs(afs) do
      if af['label']==script_name then
	 return i
      end
   end
end

function append_drc(afs)
   afs[#afs+1]={
      name="drc",
      label=script_name,
      params={
	 method="1",
	 target="0.25"
      }
   }
   print_state(afs[#afs]['params'])
end

function modify_or_create_af(fun)
   afs=mp.get_property_native('af')
   i=get_index_of_drc(afs)
   if not i then
      append_drc(afs)
   else
      fun(afs,i)
   end
   mp.set_property_native('af',afs)
end

function drc_toggle_method_handler()
   modify_or_create_af(
      function (afs,i)
	 new_method=(afs[i]['params']['method']+1)%3
	 if new_method==0 then
	    table.remove(afs,i)
	    print_state(nil)
	 else
	    afs[i]['params']['method']=tostring((afs[i]['params']['method'])%2+1)
	    print_state(afs[i]['params'])
	 end
      end
   )
end

function drc_scale_target(factor)
   modify_or_create_af(
      function (afs)
	 afs[i]['params']['target']=tostring(afs[i]['params']['target']*factor)
	 print_state(afs[i]['params'])
      end
   )
end

function drc_louder_handler()
   drc_scale_target(2.0)
end

function drc_quieter_handler()
   drc_scale_target(0.5)
end

-- toggle between off, method 1 and method 2 
mp.add_key_binding("\\","drc_toggle_method",drc_toggle_method_handler)
-- increase or decrease target volume
mp.add_key_binding("ctrl+9","drc_quieter",drc_quieter_handler)
mp.add_key_binding("ctrl+0","drc_louder",drc_louder_handler)

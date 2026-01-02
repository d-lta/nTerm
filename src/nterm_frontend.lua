nrequire "nterm"
local ok, mod = pcall(nrequire, "nterm")

if ok and type(mod)=="table" then nterm = mod end
  local nspirerc_executed = false
  local t_unpack = (table and table.unpack) or unpack
  if not t_unpack then
    t_unpack = function(t, i, j)
    i = i or 1; j = j or #t
    if i > j then return end
      return t[i], t_unpack(t, i+1, j)
      end
      end

      local ESC = string.char(27)

      local term = {
        buf = {}, line = "", hist = {}, hidx = 1, curx = 1,
        blink = true, scroll = 0, rows = 15, line_h = 14,
        font_face = "sansserif", font_style = "r", font_size = 10,
        fg = {0,255,0}, bg = {0,0,0}, draw_bg = true,
        tab_state = { last_key_line="", last_caret=1, showed_list=false, candidates=nil },
        scroll_mode = false
      }

      local nano = {
        active=false, path=nil, rows={}, row=1, col=1,
        vscroll=0, status="", msg_timer=0, dirty=false,
        wrap=true, query=nil, cmdline=nil, cutbuf=nil
      }

      local run_command

      local function clamp(v,a,b) return math.max(a, math.min(b, v)) end
      local function setc(gc,c) gc:setColorRGB(c[1],c[2],c[3]) end
      local function noansi(s) return (s:gsub(ESC .. "%[[0-9;]*m","")) end

      local FG = {
        [30]={0,0,0}, [31]={194,54,33}, [32]={37,188,36}, [33]={173,173,39},
        [34]={73,46,225}, [35]={211,56,211}, [36]={51,187,200}, [37]={203,204,205},
        [90]={129,131,131}, [91]={252,57,31}, [92]={49,231,34}, [93]={234,236,35},
        [94]={88,51,255}, [95]={249,53,248}, [96]={20,240,240}, [97]={233,235,235},
      }
      local BG = {
        [40]={0,0,0}, [41]={194,54,33}, [42]={37,188,36}, [43]={173,173,39},
        [44]={73,46,225}, [45]={211,56,211}, [46]={51,187,200}, [47]={203,204,205},
        [100]={129,131,131}, [101]={252,57,31}, [102]={49,231,34}, [103]={234,236,35},
        [104]={88,51,255}, [105]={249,53,248}, [106]={20,240,240}, [107]={233,235,235},
      }

      local function segs_from_ansi(s, dfg)
      local segs, i = {}, 1
      local cfg = {dfg[1], dfg[2], dfg[3]}
      local cbg = nil
      while i <= #s do
        local j,k,codes = s:find(ESC .. "%[([0-9;]*)m", i)
        if not j then table.insert(segs,{text=s:sub(i),fg=cfg,bg=cbg}); break end
          if j>i then table.insert(segs,{text=s:sub(i,j-1),fg=cfg,bg=cbg}) end
            local list = {}; for n in codes:gmatch("%d+") do list[#list+1]=tonumber(n) end
            if #list==0 then list={0} end
              for _,c in ipairs(list) do
                if c==0 then cfg={dfg[1],dfg[2],dfg[3]}; cbg=nil
                  elseif FG[c] then cfg=FG[c]
                    elseif BG[c] then cbg=BG[c] end
                      end
                      i = k + 1
                      end
                      return segs
                      end

                      local function draw_ansi(gc, x, y, s, dfg)
                      local cx = x
                      for _,seg in ipairs(segs_from_ansi(s, dfg)) do
                        if seg.text ~= "" then
                          local w = gc:getStringWidth(seg.text)
                          if term.draw_bg and seg.bg then setc(gc, seg.bg); gc:fillRect(cx, y-(term.line_h-2), w, term.line_h-2) end
                            setc(gc, seg.fg or dfg); gc:drawString(seg.text, cx, y); cx = cx + w
                            end
                            end
                            return cx - x
                            end

                            local function find_break_point(gc, s, max_w)
                            local w, i, last_space = 0, 1, 0
                            while i <= #s do
                              local a,b = s:find(ESC .. "%[([0-9;]*)m", i)
                              if a == i then
                                i = b + 1
                                else
                                  local ch = s:sub(i,i); if ch==" " then last_space = i end
                                  local cw = gc:getStringWidth(ch)
                                  if w + cw > max_w then return last_space>0 and last_space or i-1 end
                                    w = w + cw; i = i + 1
                                    end
                                    end
                                    return #s
                                    end

                                    local function add_line(s)
                                    term.buf[#term.buf+1] = s
                                    while #term.buf > 800 do table.remove(term.buf, 1) end
                                      end
                                      local function add(text)
                                      text = tostring(text or "")
                                      local pushed=false
                                      for line in text:gmatch("([^\r\n]+)") do add_line(line); pushed=true end
                                        if not pushed and text ~= "" then add_line(text) end
                                          end
                                          local function add_and_reset(text)
                                          add(text)
                                          term.scroll = 0
                                          end
                                          local function print_repl_results(...)
                                          local results = {...}
                                          if #results == 0 then return end

                                            local output = {}
                                            for i, v in ipairs(results) do
                                              output[i] = tostring(v)
                                              end
                                              add_and_reset(table.concat(output, "\t"))
                                              end

                                          local function update_layout()
                                          local h = platform.window:height() or 240
                                          term.line_h = math.max(12, term.font_size + 4)
                                          term.rows = math.max(3, math.floor(h / term.line_h))
                                          end
                                          local function set_font(gc) gc:setFont(term.font_face, term.font_style, term.font_size) end

                                          local function render_lines()
                                          local lines = {}; for i=1,#term.buf do lines[i]=term.buf[i] end
                                          local cwd = (nterm and nterm.getcwd and nterm.getcwd()) or "/documents"
                                          local prompt = "nterm:" .. cwd .. "$ "

                                          if not nspirerc_executed then
                                            nspirerc_executed = true

                                            add("\27[97mnterm v1.3 - Lua+C Shell + mini-nano\27[0m")
                                            add("cwd: " .. cwd)
                                            add("Type 'help' for commands. Use 'nano <path>' to edit files.")

                                            local rc_path = cwd .. "/nspirerc"
                                            if nterm and nterm.stat and nterm.exec then
                                              local ok_stat, st = pcall(nterm.stat, rc_path)
                                              if ok_stat and type(st) == "table" and st.isfile then
                                                local old_print = _G.print
                                                local captured_output = {}

                                                _G.print = function(...)
                                                local t = {}
                                                for i=1,select("#", ...) do t[i] = tostring(select(i, ...)) end
                                                  captured_output[#captured_output+1] = table.concat(t, "\t")
                                                  end

                                                  local ok, rc_or_err = pcall(function()
                                                  return nterm.exec(rc_path)
                                                  end)

                                                  _G.print = old_print

                                                  for i=1,#captured_output do
                                                    add(captured_output[i])
                                                    end

                                                    if not ok then
                                                      add("\27[91mError in .nspirerc: " .. tostring(rc_or_err) .. "\27[0m")
                                                      end
                                                      end
                                                      end

                                                      lines = {}; for i=1,#term.buf do lines[i]=term.buf[i] end
                                                      end

                                                      lines[#lines+1] = prompt .. term.line
                                                      return lines, prompt
                                                      end

                                                      local function nano_status(msg, sec) nano.status = msg or ""; nano.msg_timer = sec or 0 end

                                                      local function nano_load(path)
                                                      nano.rows = {}
                                                      local data = ""
                                                      if nterm and type(nterm.readfile)=="function" then
                                                        local ok, s = pcall(nterm.readfile, path)
                                                        if ok and type(s)=="string" then data = s end
                                                          end
                                                          for line in (data.."\n"):gmatch("([^\r\n]*)\r?\n") do table.insert(nano.rows, line) end
                                                            if #nano.rows == 0 then nano.rows = {""} end
                                                              nano.row, nano.col, nano.vscroll = 1, 1, 0
                                                              nano.dirty = false
                                                              nano_status("Read " .. path, 1.5)
                                                              end

                                                              local function nano_save()
                                                              local content = table.concat(nano.rows, "\n") .. "\n"
                                                              if not (nterm and type(nterm.nano)=="function") then
                                                                nano_status("nterm.nano unavailable", 2.5); return false
                                                                end
                                                                local ok, err = pcall(nterm.nano, nano.path, content)
                                                                if ok then nano.dirty=false; nano_status("Wrote " .. nano.path, 1.5); return true
                                                                  else nano_status("Write failed: "..tostring(err), 3.0); return false end
                                                                    end

                                                                    local function cur_line() return nano.rows[nano.row] end
                                                                    local function set_line(s) nano.rows[nano.row]=s end
                                                                    local function insert_text(s)
                                                                    local L = cur_line()
                                                                    local a = L:sub(1, nano.col-1)
                                                                    local b = L:sub(nano.col)
                                                                    set_line(a .. s .. b)
                                                                    nano.col = nano.col + #s
                                                                    nano.dirty = true
                                                                    end
                                                                    local function newline()
                                                                    local L = cur_line()
                                                                    local a = L:sub(1, nano.col-1)
                                                                    local b = L:sub(nano.col)
                                                                    set_line(a)
                                                                    table.insert(nano.rows, nano.row+1, b)
                                                                    nano.row = nano.row + 1
                                                                    nano.col = 1
                                                                    nano.dirty = true
                                                                    end
                                                                    local function backspace()
                                                                    if nano.col > 1 then
                                                                      local L = cur_line()
                                                                      local a = L:sub(1, nano.col-2)
                                                                      local b = L:sub(nano.col)
                                                                      set_line(a..b); nano.col = nano.col - 1; nano.dirty = true
                                                                      elseif nano.row > 1 then
                                                                        local prev = nano.rows[nano.row-1]
                                                                        local L = cur_line()
                                                                        nano.rows[nano.row-1] = prev .. L
                                                                        table.remove(nano.rows, nano.row)
                                                                        nano.row = nano.row - 1
                                                                        nano.col = #prev + 1
                                                                        nano.dirty = true
                                                                        end
                                                                        end
                                                                        local function del_key()
                                                                        local L = cur_line()
                                                                        if nano.col <= #L then
                                                                          set_line(L:sub(1, nano.col-1) .. L:sub(nano.col+1)); nano.dirty = true
                                                                          elseif nano.row < #nano.rows then
                                                                            local nextL = nano.rows[nano.row+1]
                                                                            set_line(L .. nextL); table.remove(nano.rows, nano.row+1); nano.dirty = true
                                                                            end
                                                                            end
                                                                            local function move_left() if nano.col>1 then nano.col=nano.col-1 elseif nano.row>1 then nano.row=nano.row-1; nano.col=#nano.rows[nano.row]+1 end end
                                                                            local function move_right() local L=cur_line(); if nano.col<=#L then nano.col=nano.col+1 elseif nano.row<#nano.rows then nano.row=nano.row+1; nano.col=1 end end
                                                                            local function move_up() if nano.row>1 then nano.row=nano.row-1; nano.col=math.min(nano.col, #cur_line()+1) end end
                                                                            local function move_down() if nano.row<#nano.rows then nano.row=nano.row+1; nano.col=math.min(nano.col, #cur_line()+1) end end
                                                                            local function page_up(vis) nano.row = math.max(1, nano.row - vis); nano.col = math.min(nano.col, #cur_line()+1) end
                                                                            local function page_down(vis) nano.row = math.min(#nano.rows, nano.row + vis); nano.col = math.min(nano.col, #cur_line()+1) end

                                                                            local function find_text(needle, from_row, from_col)
                                                                            if not needle or needle=="" then return nil end
                                                                              local r, c = from_row, from_col
                                                                              while r <= #nano.rows do
                                                                                local L = nano.rows[r]
                                                                                local s = (r==from_row) and c or 1
                                                                                local i = L:find(needle, s, true)
                                                                                if i then return r, i end
                                                                                  r = r + 1; c = 1
                                                                                  end
                                                                                  return nil
                                                                                  end

                                                                                  local function draw_editor(gc)
                                                                                  setc(gc, term.bg); gc:fillRect(0,0, platform.window:width() or 320, platform.window:height() or 240); set_font(gc)

                                                                                  local W = platform.window:width() or 320
                                                                                  local H = platform.window:height() or 240
                                                                                  local lh = term.line_h

                                                                                  setc(gc, {0,0,128}); gc:fillRect(0,0,W,lh)
                                                                                  setc(gc, {255,255,255})
                                                                                  local title = (" nano %s %s"):format(nano.path or "(new file)", nano.dirty and "[modified]" or "")
                                                                                  gc:drawString(title, 4, lh-2)

                                                                                  local rows_fit = math.max(1, math.floor((H - lh*2) / lh))
                                                                                  local first = clamp(nano.row - math.floor(rows_fit/2), 1, math.max(1, #nano.rows - rows_fit + 1))
                                                                                  nano.vscroll = first

                                                                                  local y = lh*2
                                                                                  for i = first, math.min(#nano.rows, first + rows_fit - 1) do
                                                                                    setc(gc, term.fg)
                                                                                    local s = nano.rows[i]
                                                                                    gc:drawString(s, 4, y + lh - 2)
                                                                                    if i == nano.row then
                                                                                      local before = s:sub(1, math.max(0, nano.col-1))
                                                                                      local cx = 4 + gc:getStringWidth(before)
                                                                                      if term.blink then setc(gc, term.fg); gc:fillRect(cx, y+2, 2, lh-4) end
                                                                                        end
                                                                                        y = y + lh
                                                                                        end

                                                                                        setc(gc, {32,32,32}); gc:fillRect(0, H-lh, W, lh)
                                                                                        setc(gc, {220,220,220})
                                                                                        local right = ("%d/%d %d:%d wrap:%s"):format(nano.row, #nano.rows, nano.row, nano.col, nano.wrap and "on" or "off")
                                                                                        local left = nano.cmdline and (":"..nano.cmdline) or (nano.status or "")
                                                                                        gc:drawString(left, 4, H-2)
                                                                                        local rw = gc:getStringWidth(right)
                                                                                        gc:drawString(right, W - rw - 4, H-2)
                                                                                        end

                                                                                        local function parse_args(arr)
                                                                                        local opts, pos = {}, {}; local i = 1
                                                                                        while i <= #arr do
                                                                                          local a = arr[i]
                                                                                          if a:sub(1,2) == "--" then
                                                                                            local k,v = a:match("^%-%-([^=]+)=(.+)$")
                                                                                            if k then opts[k]=v
                                                                                              else
                                                                                                k = a:sub(3)
                                                                                                if i < #arr and arr[i+1]:sub(1,1) ~= "-" then opts[k]=arr[i+1]; i=i+1 else opts[k]=true end
                                                                                                  end
                                                                                                  elseif a:sub(1,1) == "-" and #a > 1 then
                                                                                                    for j=2,#a do opts[a:sub(j,j)] = true end
                                                                                                      else
                                                                                                        table.insert(pos, a)
                                                                                                        end
                                                                                                        i=i+1
                                                                                                        end
                                                                                                        return opts, pos
                                                                                                        end

                                                                                                        CMD = {}
                                                                                                        function register(name, fn, meta)
                                                                                                        CMD[name] = { run = fn, help = meta and meta.help or "", usage = meta and meta.usage or "", complete = meta and meta.complete }
                                                                                                        end

                                                                                                        local function make_abs(p)
                                                                                                        if p:match("^/") then return p end
                                                                                                          local cwd = (nterm and nterm.getcwd and nterm.getcwd()) or "/documents"
                                                                                                          return (cwd .. "/" .. p):gsub("//+","/")
                                                                                                          end

                                                                                                          local function shell_print(...)
                                                                                                          local t = {}
                                                                                                          for i=1,select("#", ...) do t[i] = tostring(select(i, ...)) end
                                                                                                            add(table.concat(t, "\t"))
                                                                                                            end
                                                                                                            local function shell_write(...)
                                                                                                            local s = table.concat({...})
                                                                                                            for line in (s.."\n"):gmatch("([^\r\n]*)\r?\n") do
                                                                                                              if line ~= "" then add(line) end
                                                                                                                end
                                                                                                                end

                                                                                                                local BOLD = ESC .. "[1m"
                                                                                                                local RESET = ESC .. "[0m"
                                                                                                                local CYAN = ESC .. "[96m"
                                                                                                                local WHITE = ESC .. "[97m"
                                                                                                                local GREEN = ESC .. "[92m"

                                                                                                                local function out(s)
                                                                                                                if type(add) == "function" then add(s) else print(s) end
                                                                                                                  end

                                                                                                                  local NB_LOGO = {
                                                                                                                    "           ##########                    ",
                                                                                                                    "           ##########     ####           ",
                                                                                                                    "           ##########     ####           ",
                                                                                                                    "           ##########                    ",
                                                                                                                    "           ############  #### #########  ",
                                                                                                                    "           ############  ###  #########  ",
                                                                                                                    "           ############ ####  #########  ",
                                                                                                                    "           ########     ####    #######  ",
                                                                                                                    "###################    ####     ######## ",
                                                                                                                    " ####################  ####  ############",
                                                                                                                    "   ##################  #### #############",
                                                                                                                    "    %################ ####  #############",
                                                                                                                    "     ###############  ####  #############",
                                                                                                                    "      ##############          #########  ",
                                                                                                                    "        ###     ####          #####      ",
                                                                                                                    "                 ###############         ",
                                                                                                                    "                  ############           ",
                                                                                                                    "                   ##########            ",
                                                                                                                    "                    #########            ",
                                                                                                                    "                      #######            ",
                                                                                                                    "                        ######           ",
                                                                                                                  }

                                                                                                                  local function scale_logo(lines, scale)
                                                                                                                  if not scale or scale >= 0.999 then return lines end
                                                                                                                    if scale < 0.1 then scale = 0.1 end

                                                                                                                      local h = #lines
                                                                                                                      if h == 0 then return lines end
                                                                                                                        local w = 0
                                                                                                                        for i=1,h do if #lines[i] > w then w = #lines[i] end end

                                                                                                                          local padded = {}
                                                                                                                          for i=1,h do
                                                                                                                            local s = lines[i]
                                                                                                                            if #s < w then s = s .. string.rep(" ", w - #s) end
                                                                                                                              padded[i] = s
                                                                                                                              end

                                                                                                                              local step = math.max(1, math.floor(1/scale + 0.5))
                                                                                                                              local out_lines = {}

                                                                                                                              for r=1,h,step do
                                                                                                                                local src = padded[r]
                                                                                                                                local buf = {}
                                                                                                                                for c=1,w,step do
                                                                                                                                  buf[#buf+1] = src:sub(c,c)
                                                                                                                                  end
                                                                                                                                  local line = table.concat(buf):gsub("%s+$","")
                                                                                                                                  out_lines[#out_lines+1] = line
                                                                                                                                  end

                                                                                                                                  return out_lines
                                                                                                                                  end

                                                                                                                                  local function max_len(t)
                                                                                                                                  local m=0
                                                                                                                                  for i=1,#t do local l=#t[i]; if l>m then m=l end end
                                                                                                                                    return m
                                                                                                                                    end
                                                                                                                                    local function run_lua_script(script_path, extra_args)
                                                                                                                                    local old_print = _G.print
                                                                                                                                    _G.print = shell_print
                                                                                                                                    local io_tbl = rawget(_G, "io")
                                                                                                                                    local old_write = nil
                                                                                                                                    local have_io_write = (type(io_tbl)=="table" and type(io_tbl.write)=="function")
                                                                                                                                    if have_io_write then old_write = io_tbl.write; io_tbl.write = shell_write end

                                                                                                                                      local ok_exec, err_exec = pcall(function()
                                                                                                                                      return nterm.exec(script_path, t_unpack(extra_args))
                                                                                                                                      end)

                                                                                                                                      _G.print = old_print
                                                                                                                                      if have_io_write then io_tbl.write = old_write end

                                                                                                                                        if not ok_exec then
                                                                                                                                          add_and_reset("Error running script: " .. tostring(err_exec))
                                                                                                                                          end
                                                                                                                                          end
                                                                                                                                          local function fixed(s, n)
                                                                                                                                          local l = #s
                                                                                                                                          if l == n then return s
                                                                                                                                            elseif l > n then return s:sub(1, n)
                                                                                                                                              else return s .. string.rep(" ", n - l)
                                                                                                                                                end
                                                                                                                                                end

                                                                                                                                                local function parse_argv(argv)
                                                                                                                                                local opts = { logo=true, scale=1.0 }
                                                                                                                                                for _,a in ipairs(argv or {}) do
                                                                                                                                                  if a == "--no-logo" then
                                                                                                                                                    opts.logo = false
                                                                                                                                                    else
                                                                                                                                                      local k,v = a:match("^%-%-(%w+)%=(.+)$")
                                                                                                                                                      if k == "scale" then
                                                                                                                                                        local s = tonumber(v)
                                                                                                                                                        if s and s > 0 and s <= 1 then opts.scale = s end
                                                                                                                                                          end
                                                                                                                                                          end
                                                                                                                                                          end
                                                                                                                                                          return opts
                                                                                                                                                          end
                                                                                                                                                          local function cmd_lua(argv)
                                                                                                                                                          local expression = table.concat(argv, " ")

                                                                                                                                                          if expression == "" then
                                                                                                                                                            add_and_reset("\27[33mWarning: Interactive REPL not implemented. Use 'lua <expression>'.\27[0m")
                                                                                                                                                            return
                                                                                                                                                            end

                                                                                                                                                            local ok, res1, res2, res3, res4, res5, res6, res7, res8 = pcall(nterm.eval, expression)

                                                                                                                                                            if ok then
                                                                                                                                                              print_repl_results(res1, res2, res3, res4, res5, res6, res7, res8)
                                                                                                                                                              else
                                                                                                                                                                add_and_reset("\27[91mLua Error: "..tostring(res1).."\27[0m")
                                                                                                                                                                end
                                                                                                                                                                end


                                                                                                                                                          local function render_neofetch(opts)
                                                                                                                                                          if not (nterm and type(nterm.sysinfo)=="function") then
                                                                                                                                                            out("neofetch: nterm.sysinfo() not available")
                                                                                                                                                            return
                                                                                                                                                            end

                                                                                                                                                            local info = nterm.sysinfo() or {}

                                                                                                                                                            local logo_lines = {}
                                                                                                                                                            if opts.logo then
                                                                                                                                                              logo_lines = scale_logo(NB_LOGO, opts.scale)
                                                                                                                                                              for _, line in ipairs(logo_lines) do
                                                                                                                                                                out(line)
                                                                                                                                                                end
                                                                                                                                                                end

                                                                                                                                                                out("")

                                                                                                                                                                out(BOLD .. WHITE .. "nspire" .. RESET .. "@" .. GREEN .. "calculator" .. RESET)
                                                                                                                                                                out(WHITE .. "--------------------------" .. RESET)

                                                                                                                                                                local labels = {"OS", "Host", "Chip", "CPU", "Memory", "Screen", "Battery", "Power", "Charging", "Battery%", "Colors"}
                                                                                                                                                                local values = {
                                                                                                                                                                  info.os or "Unknown",
                                                                                                                                                                  info.model or "Unknown",
                                                                                                                                                                  info.chip or "Unknown",
                                                                                                                                                                  info.cpu or "Unknown",
                                                                                                                                                                  info.memory or "Unknown",
                                                                                                                                                                  info.screen or "Unknown",
                                                                                                                                                                  info.battery or "N/A",
                                                                                                                                                                  info.power or "Unknown",
                                                                                                                                                                  info.charging or "Unknown",
                                                                                                                                                                  info.battery_percent or "N/A",
                                                                                                                                                                  info.colors or (info.has_colors and "Yes" or "No"),
                                                                                                                                                                }

                                                                                                                                                                local labw = 0
                                                                                                                                                                for i=1,#labels do local L=#labels[i]; if L > labw then labw = L end end

                                                                                                                                                                  for i=1, #labels do
                                                                                                                                                                    local right = CYAN .. string.format("%-" .. labw .. "s", labels[i]) .. RESET ..
                                                                                                                                                                    " : " .. tostring(values[i])
                                                                                                                                                                    out(right)
                                                                                                                                                                    end

                                                                                                                                                                    if (info.colors == "Yes") or info.has_colors == true then
                                                                                                                                                                      out("")
                                                                                                                                                                      out(WHITE .. " " .. RESET ..
                                                                                                                                                                      ESC .. "[91m " .. RESET .. ESC .. "[92m " .. RESET ..
                                                                                                                                                                      ESC .. "[93m " .. RESET .. ESC .. "[94m " .. RESET ..
                                                                                                                                                                      ESC .. "[96m " .. RESET)
                                                                                                                                                                      end
                                                                                                                                                                      end
                                                                                                                                                                      register("exec", function(argv)
                                                                                                                                                                      local _, pos = parse_args(argv)
                                                                                                                                                                      local p = pos[1]
                                                                                                                                                                      if not p then add_and_reset("Usage: exec <path> [args...]"); return end

                                                                                                                                                                        local script_path = make_abs(p)

                                                                                                                                                                        local old_print = _G.print
                                                                                                                                                                        _G.print = shell_print
                                                                                                                                                                        local io_tbl = rawget(_G, "io")
                                                                                                                                                                        local old_write = nil
                                                                                                                                                                        local have_io_write = (type(io_tbl)=="table" and type(io_tbl.write)=="function")
                                                                                                                                                                        if have_io_write then old_write = io_tbl.write; io_tbl.write = shell_write end

                                                                                                                                                                          local extra = {}
                                                                                                                                                                          for i=2,#pos do extra[#extra+1] = pos[i] end

                                                                                                                                                                            local ok, rc_or_err = pcall(function()
                                                                                                                                                                            return nterm.exec(script_path, t_unpack(extra))
                                                                                                                                                                            end)

                                                                                                                                                                            _G.print = old_print
                                                                                                                                                                            if have_io_write then io_tbl.write = old_write end

                                                                                                                                                                              if not ok then
                                                                                                                                                                                add_and_reset("exec: " .. tostring(rc_or_err))
                                                                                                                                                                                else
                                                                                                                                                                                  add_and_reset("exec: " .. tostring(rc_or_err))
                                                                                                                                                                                  end
                                                                                                                                                                                  end, {help="Execute a .lua (in-VM) or .tns (via nl_exec)", usage="exec <path>..."})

                                                                                                                                                                      register("scroll", function(argv)
                                                                                                                                                                      local _, pos = parse_args(argv)
                                                                                                                                                                      local direction = pos[1] or ""
                                                                                                                                                                      local amount = tonumber(pos[2]) or 1

                                                                                                                                                                      if direction == "up" then
                                                                                                                                                                        term.scroll = term.scroll + amount
                                                                                                                                                                        elseif direction == "down" then
                                                                                                                                                                          term.scroll = math.max(0, term.scroll - amount)
                                                                                                                                                                          elseif direction == "top" then
                                                                                                                                                                            term.scroll = 999999  -- Will be clamped by the drawing function
                                                                                                                                                                            elseif direction == "bottom" then
                                                                                                                                                                              term.scroll = 0
                                                                                                                                                                              else
                                                                                                                                                                                add_and_reset("Usage: scroll up|down|top|bottom [amount]")
                                                                                                                                                                                add("Examples:")
                                                                                                                                                                                add("  scroll up 5  - scroll up 5 lines")
                                                                                                                                                                                add("  scroll down 3  - scroll down 3 lines")
                                                                                                                                                                                add("  scroll top   - scroll to very top")
                                                                                                                                                                                add("  scroll bottom - scroll to bottom")
                                                                                                                                                                                return
                                                                                                                                                                                end

                                                                                                                                                                                platform.window:invalidate()
                                                                                                                                                                                end, {help="Scroll terminal output", usage="scroll up|down|top|bottom [amount]"})

                                                                                                                                                                      register("echo", function(argv) add_and_reset(table.concat(select(2, parse_args(argv)), " ")) end, {help="Print text", usage="echo text..."})
                                                                                                                                                                      register("clear", function() term.buf = {} end, {help="Clear screen"})
                                                                                                                                                                      register("lua", cmd_lua, {help="Evaluate Lua expression", usage="lua <expression>"})
                                                                                                                                                                      register("history", function() add_and_reset("Command history:"); for i,h in ipairs(term.hist) do add_and_reset((" %d: %s"):format(i,h)) end end, {help="Show command history"})
                                                                                                                                                                      register("theme", function(argv)
                                                                                                                                                                      local n = (select(2, parse_args(argv)))[1] or ""; n=n:lower()
                                                                                                                                                                      if n=="classic" then term.bg={0,0,0}; term.fg={0,255,0}
                                                                                                                                                                      elseif n=="amber" then term.bg={0,0,0}; term.fg={255,191,0}
                                                                                                                                                                      elseif n=="paper" then term.bg={245,245,242}; term.fg={34,34,34}
                                                                                                                                                                      elseif n=="mint" then term.bg={5,35,35}; term.fg={178,255,218}
                                                                                                                                                                      else add_and_reset("theme: classic | amber | paper | mint") end
                                                                                                                                                                        end, {help="Change theme"})
                                                                                                                                                                      register("neofetch", function(argv)
                                                                                                                                                                      local opts = parse_argv(argv)
                                                                                                                                                                      render_neofetch(opts)
                                                                                                                                                                      add_and_reset("")
                                                                                                                                                                      end, {help="Display system information", usage="neofetch [--no-logo] [--scale=N]"})
                                                                                                                                                                      register("ls", function(argv)
                                                                                                                                                                      local opts, pos = parse_args(argv)

                                                                                                                                                                      local ls_opts = {
                                                                                                                                                                        path = pos[1] or ".",
                                                                                                                                                                        long = opts.l,
                                                                                                                                                                        all = opts.a,
                                                                                                                                                                        ["dirs-first"] = opts["dirs-first"]
                                                                                                                                                                      }

                                                                                                                                                                      local ok2, res = pcall(nterm.ls, ls_opts)
                                                                                                                                                                      if ok2 and res then add_and_reset(res) else add_and_reset("ls: " .. tostring(res)) end
                                                                                                                                                                        end, {help="List files", usage="ls [-a] [-l] [--dirs-first] [path]"})

                                                                                                                                                                      register("mkdir", function(argv)
                                                                                                                                                                      local _, pos = parse_args(argv); if #pos == 0 then add_and_reset("mkdir: missing operand"); return end
                                                                                                                                                                      for _, path in ipairs(pos) do local ok2, err2 = pcall(nterm.mkdir, path); if not ok2 then add_and_reset("mkdir: "..path..": " .. tostring(err2)) end end
                                                                                                                                                                        add_and_reset("")
                                                                                                                                                                        end, {help="Create directories", usage="mkdir <dir>..."})

                                                                                                                                                                      register("rm", function(argv)
                                                                                                                                                                      local opts, pos = parse_args(argv); if #pos == 0 then add_and_reset("rm: missing operand"); return end
                                                                                                                                                                      local ok2, err2 = pcall(nterm.rm, {paths = pos, force = opts.f or false, recursive = opts.r or false})
                                                                                                                                                                      if not ok2 then add_and_reset("rm: " .. tostring(err2)) end
                                                                                                                                                                        add_and_reset("")
                                                                                                                                                                        end, {help="Remove files or directories", usage="rm [-f] [-r] <file>..."})

                                                                                                                                                                      register("help", function(argv)
                                                                                                                                                                      local pos = select(2, parse_args(argv)); if pos[1] and CMD[pos[1]] then
                                                                                                                                                                      local m = CMD[pos[1]]
                                                                                                                                                                      add_and_reset((m.usage ~= "" and ("Usage: "..m.usage) or ("Command: "..pos[1])))
                                                                                                                                                                      if m.help ~= "" then add_and_reset(m.help) end
                                                                                                                                                                        return
                                                                                                                                                                        end
                                                                                                                                                                        add_and_reset("Builtins (Lua):"); local builtins = {}; for k in pairs(CMD) do builtins[#builtins+1]=k end
                                                                                                                                                                        table.sort(builtins); add_and_reset(" "..table.concat(builtins, ", "))
                                                                                                                                                                        if nterm then
                                                                                                                                                                          add_and_reset("Builtins (C):"); local c_cmds = {}
                                                                                                                                                                          for k,v in pairs(nterm) do if type(v) == "function" and not CMD[k] then c_cmds[#c_cmds+1]=k end end
                                                                                                                                                                            table.sort(c_cmds); if #c_cmds > 0 then add_and_reset(" "..table.concat(c_cmds, ", ")) end
                                                                                                                                                                            end
                                                                                                                                                                            add_and_reset("Try: help <command>")
                                                                                                                                                                            end, {help="Show help", usage="help [command]"})

                                                                                                                                                                      register("exit", function() add_and_reset("Goodbye!") end, {help="Exit shell"})

                                                                                                                                                                      register("nano", function(argv)
                                                                                                                                                                      local _, pos = parse_args(argv)
                                                                                                                                                                      local path = pos[1]
                                                                                                                                                                      if not path or path == "" then add_and_reset("Usage: nano <path>"); return end
                                                                                                                                                                        nano.path = path
                                                                                                                                                                        nano.active = true
                                                                                                                                                                        nano.cmdline = nil
                                                                                                                                                                        nano.cutbuf = nil
                                                                                                                                                                        nano.wrap = true
                                                                                                                                                                        nano_load(path)
                                                                                                                                                                        platform.window:invalidate()
                                                                                                                                                                        end, {help="Open a mini-nano editor", usage="nano <path>"})

                                                                                                                                                                      local function tokenize_line(s) local t = {}; for tok in s:gmatch("%S+") do t[#t+1]=tok end; return t end
                                                                                                                                                                      run_command = function(line)
                                                                                                                                                                      local cwd = (nterm and nterm.getcwd and nterm.getcwd()) or "/documents"
                                                                                                                                                                      add("nterm:" .. cwd .. "$ " .. line)
                                                                                                                                                                      if line:match("%S") then term.hist[#term.hist+1] = line; term.hidx = #term.hist + 1 end
                                                                                                                                                                        local toks = tokenize_line(line); if #toks == 0 then term.scroll = 0; return end
                                                                                                                                                                        local cmd = toks[1]; local argv = {}; for i = 2, #toks do argv[#argv+1] = toks[i] end

                                                                                                                                                                        if CMD[cmd] then
                                                                                                                                                                          CMD[cmd].run(argv)
                                                                                                                                                                          elseif nterm and type(nterm[cmd]) == "function" then
                                                                                                                                                                            local ok2, res = pcall(nterm[cmd], t_unpack(argv))
                                                                                                                                                                            if not ok2 then
                                                                                                                                                                              add_and_reset("nterm:"..cmd..": "..tostring(res))
                                                                                                                                                                              elseif cmd ~= "exec" and res ~= nil then
                                                                                                                                                                                add_and_reset(tostring(res))
                                                                                                                                                                                end
                                                                                                                                                                                else
                                                                                                                                                                                  local script_path = "/documents/nterm/bin/" .. cmd .. ".lua"
                                                                                                                                                                                  local stat_info = nterm.stat and nterm.stat(script_path)
                                                                                                                                                                                  if type(stat_info) == "table" and stat_info.isfile then
                                                                                                                                                                                    run_lua_script(script_path, argv)
                                                                                                                                                                                    else
                                                                                                                                                                                      add_and_reset("nterm: command not found: " .. cmd)
                                                                                                                                                                                      end
                                                                                                                                                                                      end
if platform.window and type(platform.window.draw) == "function" then
        platform.window:draw()
    end
                                                                                                                                                                                      end

                                                                                                                                                                                      local function draw_shell(gc)
                                                                                                                                                                                      setc(gc, term.bg); gc:fillRect(0,0, platform.window:width() or 320, platform.window:height() or 240); set_font(gc)
                                                                                                                                                                                      local lines, prompt = render_lines(); local all_wrapped_lines = {}
                                                                                                                                                                                      local screen_w = platform.window:width() or 320
                                                                                                                                                                                      for i=1, #lines do
                                                                                                                                                                                        local line_to_wrap = lines[i]
                                                                                                                                                                                        while #line_to_wrap > 0 do
                                                                                                                                                                                          local break_idx = find_break_point(gc, line_to_wrap, screen_w - 2)
                                                                                                                                                                                          table.insert(all_wrapped_lines, line_to_wrap:sub(1, break_idx))
                                                                                                                                                                                          line_to_wrap = line_to_wrap:sub(break_idx + 1)
                                                                                                                                                                                          end
                                                                                                                                                                                          end
                                                                                                                                                                                          local total = #all_wrapped_lines; local win_rows = term.rows; local max_scroll = math.max(0, total - win_rows)
                                                                                                                                                                                          term.scroll = clamp(term.scroll, 0, max_scroll)
                                                                                                                                                                                          local start_idx = math.max(1, total - win_rows - term.scroll + 1); local end_idx = math.min(total, start_idx + win_rows -1)
                                                                                                                                                                                          local y = term.line_h
                                                                                                                                                                                          for i = start_idx, end_idx do
                                                                                                                                                                                            if all_wrapped_lines[i] then draw_ansi(gc, 1, y, all_wrapped_lines[i], term.fg); y = y + term.line_h end
                                                                                                                                                                                              end

                                                                                                                                                                                              if term.scroll == 0 then
                                                                                                                                                                                                local before_caret = noansi(prompt .. term.line:sub(1, term.curx - 1))
                                                                                                                                                                                                local cx = 1 + gc:getStringWidth(before_caret)
                                                                                                                                                                                                if term.blink then setc(gc, term.fg); gc:fillRect(cx, y - term.line_h - (term.line_h - 2), 2, term.line_h - 3) end
                                                                                                                                                                                                  end
                                                                                                                                                                                                  end

                                                                                                                                                                                                  local function draw(gc)
                                                                                                                                                                                                  if nano.active then draw_editor(gc) else draw_shell(gc) end
                                                                                                                                                                                                    end

                                                                                                                                                                                                    function on.resize() update_layout(); platform.window:invalidate() end
                                                                                                                                                                                                    function on.paint(gc) draw(gc) end

                                                                                                                                                                                                    function on.timer()
                                                                                                                                                                                                    term.blink = not term.blink
                                                                                                                                                                                                    if nano.active and nano.msg_timer and nano.msg_timer>0 then
                                                                                                                                                                                                      nano.msg_timer = nano.msg_timer - 0.5
                                                                                                                                                                                                      if nano.msg_timer <= 0 then nano.status = "" end
                                                                                                                                                                                                        end
                                                                                                                                                                                                        platform.window:invalidate()
                                                                                                                                                                                                        end

                                                                                                                                                                                                        local function editor_enter()
                                                                                                                                                                                                        if nano.cmdline ~= nil then
                                                                                                                                                                                                          local cmd = nano.cmdline; nano.cmdline = nil
                                                                                                                                                                                                          cmd = cmd:gsub("^%s+",""):gsub("%s+$","")
                                                                                                                                                                                                          if cmd == "" then return end
                                                                                                                                                                                                            if cmd:sub(1,1) == "/" then
                                                                                                                                                                                                              local q = cmd:sub(2)
                                                                                                                                                                                                              local r,c = find_text(q, nano.row, nano.col)
                                                                                                                                                                                                              if r then nano.row=r; nano.col=c; nano_status("Found: "..q,1.0) else nano_status("Not found: "..q,1.5) end
                                                                                                                                                                                                                return
                                                                                                                                                                                                                end
                                                                                                                                                                                                                local name, arg = cmd:match("^([a-zA-Z]+)%s*(.*)$"); name = (name or ""):lower()
                                                                                                                                                                                                                if name == "w" or name=="write" or name=="writeout" then nano_save()
                                                                                                                                                                                                                  elseif name == "q" or name=="quit" then
                                                                                                                                                                                                                    if nano.dirty then nano_status("Unsaved changes. Use :q! or :wq", 2.0) else nano.active=false end
                                                                                                                                                                                                                      elseif name == "wq" or name=="x" then if nano_save() then nano.active=false end
                                                                                                                                                                                                                        elseif name == "q!" then nano.active=false
                                                                                                                                                                                                                          elseif name == "set" then
                                                                                                                                                                                                                            local k,v = arg:match("([%w_]+)%s+(%w+)")
                                                                                                                                                                                                                            if (k or ""):lower()=="wrap" then nano.wrap = (v or ""):lower()=="on"; nano_status("wrap="..(nano.wrap and "on" or "off"), 1.0) end
                                                                                                                                                                                                                              elseif name == "where" then
                                                                                                                                                                                                                                local q = arg
                                                                                                                                                                                                                                local r,c = find_text(q, nano.row, nano.col)
                                                                                                                                                                                                                                if r then nano.row=r; nano.col=c; nano_status("Found: "..q,1.0) else nano_status("Not found: "..q,1.5) end
                                                                                                                                                                                                                                  elseif name == "help" or name=="h" then
                                                                                                                                                                                                                                    nano_status("Commands: :w :q :wq :q! :help /text :where text :set wrap on|off", 4.0)
                                                                                                                                                                                                                                    else
                                                                                                                                                                                                                                      nano_status("Unknown command: "..cmd, 1.5)
                                                                                                                                                                                                                                      end
                                                                                                                                                                                                                                      return
                                                                                                                                                                                                                                      end
                                                                                                                                                                                                                                      newline()
                                                                                                                                                                                                                                      end

                                                                                                                                                                                                                                      function on.charIn(ch)
                                                                                                                                                                                                                                      if nano.active then
                                                                                                                                                                                                                                        if nano.cmdline ~= nil then
                                                                                                                                                                                                                                          nano.cmdline = nano.cmdline .. ch; platform.window:invalidate(); return
                                                                                                                                                                                                                                          end
                                                                                                                                                                                                                                          if ch == ":" and nano.cmdline == nil then
                                                                                                                                                                                                                                            nano.cmdline = ""; nano_status("Command:", 0); platform.window:invalidate(); return
                                                                                                                                                                                                                                            end
                                                                                                                                                                                                                                            if ch == "\t" then ch = " " end
                                                                                                                                                                                                                                              insert_text(ch); platform.window:invalidate(); return
                                                                                                                                                                                                                                              end

                                                                                                                                                                                                                                              if ch == "\t" then on.tabKey(); return end

                                                                                                                                                                                                                                                local L = term.line:sub(1, term.curx-1)
                                                                                                                                                                                                                                                term.line = L .. ch .. term.line:sub(term.curx)
                                                                                                                                                                                                                                                term.curx = term.curx + #ch
                                                                                                                                                                                                                                                term.blink = true; term.scroll = 0; term.tab_state.showed_list=false
                                                                                                                                                                                                                                                platform.window:invalidate()
                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                function on.backspaceKey()
                                                                                                                                                                                                                                                if nano.active then
                                                                                                                                                                                                                                                  if nano.cmdline ~= nil then
                                                                                                                                                                                                                                                    if #nano.cmdline>0 then nano.cmdline = nano.cmdline:sub(1, -2) else nano.cmdline = nil end
                                                                                                                                                                                                                                                      platform.window:invalidate(); return
                                                                                                                                                                                                                                                      end
                                                                                                                                                                                                                                                      backspace(); platform.window:invalidate(); return
                                                                                                                                                                                                                                                      end
                                                                                                                                                                                                                                                      if term.curx > 1 then
                                                                                                                                                                                                                                                        local L = term.line:sub(1, term.curx-2); term.line = L .. term.line:sub(term.curx); term.curx = term.curx - 1
                                                                                                                                                                                                                                                        term.blink = true; term.scroll = 0; term.tab_state.showed_list=false
                                                                                                                                                                                                                                                        platform.window:invalidate()
                                                                                                                                                                                                                                                        end
                                                                                                                                                                                                                                                        end

                                                                                                                                                                                                                                                        function on.enterKey()
                                                                                                                                                                                                                                                        if nano.active then editor_enter(); platform.window:invalidate(); return end
                                                                                                                                                                                                                                                          run_command(term.line); term.line=""; term.curx=1; term.blink=true; term.tab_state.showed_list=false; platform.window:invalidate()
                                                                                                                                                                                                                                                          end

                                                                                                                                                                                                                                                          function on.escapeKey()
                                                                                                                                                                                                                                                          if nano.active then
                                                                                                                                                                                                                                                            if nano.cmdline ~= nil then nano.cmdline = nil; nano_status("Cancelled", 0.8)
                                                                                                                                                                                                                                                              else
                                                                                                                                                                                                                                                                if nano.dirty then nano_status("Unsaved changes. Use :wq to save & quit, or :q! to discard.", 3.0)
                                                                                                                                                                                                                                                                else nano.active=false end
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                platform.window:invalidate(); return
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                run_command("exit"); platform.window:invalidate()
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                function on.arrowLeft()
                                                                                                                                                                                                                                                                if nano.active then move_left(); platform.window:invalidate(); return end
                                                                                                                                                                                                                                                                if term.curx>1 then term.curx=term.curx-1; term.blink=true; term.tab_state.showed_list=false; platform.window:invalidate() end
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                function on.arrowRight()
                                                                                                                                                                                                                                                                if nano.active then move_right(); platform.window:invalidate(); return end
                                                                                                                                                                                                                                                                if term.curx<=#noansi(term.line) then term.curx=term.curx+1; term.blink=true; term.tab_state.showed_list=false; platform.window:invalidate() end
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                function on.arrowUp()
                                                                                                                                                                                                                                                                if nano.active then move_up(); platform.window:invalidate(); return end
                                                                                                                                                                                                                                                                if term.scroll_mode then
                                                                                                                                                                                                                                                                term.scroll = term.scroll + 1
                                                                                                                                                                                                                                                                platform.window:invalidate()
                                                                                                                                                                                                                                                                else
                                                                                                                                                                                                                                                                if #term.hist>0 and term.hidx>1 then
                                                                                                                                                                                                                                                                term.hidx=term.hidx-1; term.line=term.hist[term.hidx]; term.curx=#noansi(term.line)+1
                                                                                                                                                                                                                                                                term.blink=true; term.scroll=0; term.tab_state.showed_list=false; platform.window:invalidate()
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                function on.arrowDown()
                                                                                                                                                                                                                                                                if nano.active then move_down(); platform.window:invalidate(); return end
                                                                                                                                                                                                                                                                if term.scroll_mode then
                                                                                                                                                                                                                                                                term.scroll = math.max(0, term.scroll - 1)
                                                                                                                                                                                                                                                                platform.window:invalidate()
                                                                                                                                                                                                                                                                else
                                                                                                                                                                                                                                                                if term.hidx<=#term.hist then
                                                                                                                                                                                                                                                                if term.hidx==#term.hist then term.hidx=term.hidx+1; term.line=""; term.curx=1
                                                                                                                                                                                                                                                                else term.hidx=term.hidx+1; term.line=term.hist[term.hidx]; term.curx=#noansi(term.line)+1 end
                                                                                                                                                                                                                                                                term.blink=true; term.scroll=0; term.tab_state.showed_list=false; platform.window:invalidate()
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                function on.pageUp() if nano.active then page_up(term.rows-2); platform.window:invalidate() end end
                                                                                                                                                                                                                                                                function on.pageDown() if nano.active then page_down(term.rows-2); platform.window:invalidate() end end

                                                                                                                                                                                                                                                                local function token_at_cursor(s, curx)
                                                                                                                                                                                                                                                                local i = math.max(1, curx - 1)
                                                                                                                                                                                                                                                                local left = i
                                                                                                                                                                                                                                                                while left > 1 and s:sub(left-1,left-1) ~= " " do left = left - 1 end
                                                                                                                                                                                                                                                                local right = i
                                                                                                                                                                                                                                                                while right <= #s and s:sub(right,right) ~= " " do right = right + 1 end
                                                                                                                                                                                                                                                                local idx, pos = 1, 1
                                                                                                                                                                                                                                                                while true do
                                                                                                                                                                                                                                                                local a,b = s:find("%S+", pos)
                                                                                                                                                                                                                                                                if not a or a > left then break end
                                                                                                                                                                                                                                                                idx = idx + 1; pos = b + 1
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                return left, right-1, s:sub(left, right-1), idx
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                local function split_path(tok)
                                                                                                                                                                                                                                                                local slash = tok:match("^(.*/)")
                                                                                                                                                                                                                                                                if slash then
                                                                                                                                                                                                                                                                local dir = slash:gsub("/+$","/")
                                                                                                                                                                                                                                                                local base = tok:sub(#dir+1)
                                                                                                                                                                                                                                                                if dir:sub(1,1) ~= "/" then
                                                                                                                                                                                                                                                                local cwd = (nterm and nterm.getcwd and nterm.getcwd()) or "/documents"
                                                                                                                                                                                                                                                                dir = (cwd .. "/" .. dir):gsub("//+","/")
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                return dir, base
                                                                                                                                                                                                                                                                else
                                                                                                                                                                                                                                                                local cwd = (nterm and nterm.getcwd and nterm.getcwd()) or "/documents"
                                                                                                                                                                                                                                                                return cwd .. "/", tok
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                local function list_dir(path)
                                                                                                                                                                                                                                                                local ok, out = pcall(nterm.ls, {path = path, all = true})
                                                                                                                                                                                                                                                                if not ok or type(out) ~= "string" then return {} end
                                                                                                                                                                                                                                                                local items = {}
                                                                                                                                                                                                                                                                for raw in (out .. "\n"):gmatch("([^\r\n]+)") do
                                                                                                                                                                                                                                                                local line = noansi(raw)
                                                                                                                                                                                                                                                                local name = line:match("^%s*(%S+)")
                                                                                                                                                                                                                                                                if name and name ~= "." and name ~= ".." then items[#items+1] = name end
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                return items
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                local function is_dir(path)
                                                                                                                                                                                                                                                                local st = nterm.stat and nterm.stat(path)
                                                                                                                                                                                                                                                                return type(st)=="table" and st.isdir == true
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                local function collect_command_candidates(prefix)
                                                                                                                                                                                                                                                                local set = {}
                                                                                                                                                                                                                                                                for k,_ in pairs(CMD) do if k:sub(1,#prefix) == prefix then set[k] = true end end
                                                                                                                                                                                                                                                                if nterm then
                                                                                                                                                                                                                                                                for k,v in pairs(nterm) do
                                                                                                                                                                                                                                                                if type(v) == "function" and k ~= "exec" and k:sub(1,#prefix) == prefix then set[k] = true end
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                local bin = "/documents/nterm/bin/"
                                                                                                                                                                                                                                                                for _,name in ipairs(list_dir(bin)) do
                                                                                                                                                                                                                                                                local disp = name
                                                                                                                                                                                                                                                                if disp:sub(-4):lower() == ".lua" then disp = disp:sub(1, -5) end
                                                                                                                                                                                                                                                                if disp:sub(1,#prefix) == prefix then set[disp] = true end
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                local out = {}; for k,_ in pairs(set) do out[#out+1] = k end
                                                                                                                                                                                                                                                                table.sort(out); return out
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                local function collect_path_candidates(tok)
                                                                                                                                                                                                                                                                local dir, base = split_path(tok)
                                                                                                                                                                                                                                                                local entries = list_dir(dir)
                                                                                                                                                                                                                                                                local out = {}
                                                                                                                                                                                                                                                                for _,name in ipairs(entries) do
                                                                                                                                                                                                                                                                if name:sub(1,#base) == base then
                                                                                                                                                                                                                                                                local full = (dir .. name):gsub("//+","/")
                                                                                                                                                                                                                                                                local suffix = is_dir(full) and "/" or ""
                                                                                                                                                                                                                                                                local repl = tok:gsub("[^/]*$", name) .. suffix
                                                                                                                                                                                                                                                                out[#out+1] = repl
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                table.sort(out)
                                                                                                                                                                                                                                                                return out
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                local function common_prefix(strings)
                                                                                                                                                                                                                                                                if #strings == 0 then return "" end
                                                                                                                                                                                                                                                                local p = strings[1]
                                                                                                                                                                                                                                                                for i=2,#strings do
                                                                                                                                                                                                                                                                local s = strings[i]
                                                                                                                                                                                                                                                                local j = 1
                                                                                                                                                                                                                                                                while j <= #p and j <= #s and p:sub(j,j) == s:sub(j,j) do j = j + 1 end
                                                                                                                                                                                                                                                                p = p:sub(1, j-1)
                                                                                                                                                                                                                                                                if p == "" then break end
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                return p
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                function on.tabKey()
                                                                                                                                                                                                                                                                if nano.active then
                                                                                                                                                                                                                                                                insert_text(" ")
                                                                                                                                                                                                                                                                platform.window:invalidate()
                                                                                                                                                                                                                                                                return
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                if term.tab_state.showed_list then
                                                                                                                                                                                                                                                                local left, right, tok, idx = token_at_cursor(term.line, term.curx)
                                                                                                                                                                                                                                                                local candidates = (idx == 1) and collect_command_candidates(tok) or collect_path_candidates(tok)
                                                                                                                                                                                                                                                                local list_str = table.concat(candidates, "\t")
                                                                                                                                                                                                                                                                add(list_str)
                                                                                                                                                                                                                                                                term.tab_state.showed_list = false
                                                                                                                                                                                                                                                                else
                                                                                                                                                                                                                                                                local left, right, tok, idx = token_at_cursor(term.line, term.curx)
                                                                                                                                                                                                                                                                local candidates = (idx == 1) and collect_command_candidates(tok) or collect_path_candidates(tok)

                                                                                                                                                                                                                                                                if #candidates == 0 then
                                                                                                                                                                                                                                                                term.tab_state.showed_list = false
                                                                                                                                                                                                                                                                platform.window:invalidate()
                                                                                                                                                                                                                                                                return
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                if #candidates == 1 then
                                                                                                                                                                                                                                                                local ins = candidates[1]
                                                                                                                                                                                                                                                                term.line = term.line:sub(1, left-1) .. ins .. term.line:sub(right+1)
                                                                                                                                                                                                                                                                term.curx = left + #ins
                                                                                                                                                                                                                                                                term.tab_state.showed_list = false
                                                                                                                                                                                                                                                                term.tab_state.last_key_line = term.line
                                                                                                                                                                                                                                                                term.tab_state.last_caret = term.curx
                                                                                                                                                                                                                                                                platform.window:invalidate()
                                                                                                                                                                                                                                                                return
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                local cp = common_prefix(candidates)
                                                                                                                                                                                                                                                                if cp and cp ~= "" and cp ~= tok then
                                                                                                                                                                                                                                                                term.line = term.line:sub(1, left-1) .. cp .. term.line:sub(right+1)
                                                                                                                                                                                                                                                                term.curx = left + #cp
                                                                                                                                                                                                                                                                term.tab_state.showed_list = false
                                                                                                                                                                                                                                                                term.tab_state.last_key_line = term.line
                                                                                                                                                                                                                                                                term.tab_state.last_caret = term.curx
                                                                                                                                                                                                                                                                platform.window:invalidate()
                                                                                                                                                                                                                                                                return
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                term.tab_state.showed_list = true
                                                                                                                                                                                                                                                                term.tab_state.last_key_line = term.line
                                                                                                                                                                                                                                                                term.tab_state.last_caret = term.curx
                                                                                                                                                                                                                                                                add_and_reset(#candidates .. " matches. Press TAB again to list.")
                                                                                                                                                                                                                                                                platform.window:invalidate()
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                function on.backtabKey()
                                                                                                                                                                                                                                                                if nano.active then
                                                                                                                                                                                                                                                                local L = cur_line()
                                                                                                                                                                                                                                                                local removed = 0
                                                                                                                                                                                                                                                                if L:sub(1,1) == " " then L = L:sub(2); removed = removed + 1 end
                                                                                                                                                                                                                                                                if L:sub(1,1) == " " then L = L:sub(2); removed = removed + 1 end
                                                                                                                                                                                                                                                                set_line(L); nano.col = math.max(1, nano.col - removed); nano.dirty = true
                                                                                                                                                                                                                                                                platform.window:invalidate()
                                                                                                                                                                                                                                                                else
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                end

                                                                                                                                                                                                                                                                function on.menuKey()
                                                                                                                                                                                                                                                                if nano.active then
                                                                                                                                                                                                                                                                nano.cmdline = ""; nano_status("Command:", 0); platform.window:invalidate(); return
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                term.scroll_mode = not term.scroll_mode
                                                                                                                                                                                                                                                                if term.scroll_mode then
                                                                                                                                                                                                                                                                add_and_reset("\27[92mScroll Mode ON\27[0m - Use Up/Down arrows to scroll.")
                                                                                                                                                                                                                                                                else
                                                                                                                                                                                                                                                                add_and_reset("\27[91mScroll Mode OFF\27[0m - Use Up/Down arrows for history.")
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                platform.window:invalidate()
                                                                                                                                                                                                                                                                end
local function tool_command_runner(toolboxName, itemName)
    -- This function dispatches system-like commands
    if toolboxName == "Shell" then
        if itemName == "Clear Screen" then
            run_command("clear")
        elseif itemName == "Show History" then
            run_command("history")
        elseif itemName == "Exit Shell" then
            run_command("exit")
        end
    elseif toolboxName == "System" then
        if itemName == "Show Info" then
            run_command("neofetch")
        elseif itemName == "Toggle Scroll Mode" then
            on.menuKey()
        end
    elseif toolboxName == "Theme" then
        local theme_map = {
            ["Classic (Green)"] = "classic",
            ["Amber"] = "amber",
            ["Paper (Light)"] = "paper",
            ["Mint"] = "mint"
        }
        local theme_name = theme_map[itemName]
        if theme_name then
            run_command("theme " .. theme_name)
        end
    end
end

local menu_structure = {
    {"Shell",
            {"Clear Screen", tool_command_runner},
            {"Show History", tool_command_runner},
            {"Exit Shell", tool_command_runner}
    },
    {"System",
            {"Show Info", tool_command_runner},
            {"Toggle Scroll Mode", tool_command_runner}
    },
    {"Theme",
            {"Classic (Green)", tool_command_runner},
            {"Amber", tool_command_runner},
            {"Paper (Light)", tool_command_runner},
            {"Mint", tool_command_runner}
    },
}


if type(toolpalette) == "table" and type(toolpalette.register) == "function" then
    local ok, err = pcall(toolpalette.register, menu_structure)
    if not ok then
        add("toolpalette registration failed: " .. tostring(err))
    end
end
                                                                                                                                                                                                                                                                do
                                                                                                                                                                                                                                                                local old_charIn = on.charIn
                                                                                                                                                                                                                                                                on.charIn = function(ch)
                                                                                                                                                                                                                                                                if nano.active and ch == ":" and nano.cmdline == nil then
                                                                                                                                                                                                                                                                nano.cmdline = ""; nano_status("Command:", 0); platform.window:invalidate(); return
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                old_charIn(ch)
                                                                                                                                                                                                                                                                end
                                                                                                                                                                                                                                                                end

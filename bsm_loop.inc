  if (getenv("BSM2_BOOT")) {
    StartStallWatch(&ctx);
    BsmSetDimensions(window.width(), window.height());
    const uint64_t activity=arc_jni_object();
    auto invoke=[&](const char* name,std::initializer_list<uint64_t> args,float x=0,float y=0) {
      const uint64_t fn=BsmNative(name);
      if(!fn){printf("[bsm] unavailable callback %s\n",name);return false;}
      memset(&ctx,0,sizeof(ctx));ctx.sp=stack_top;ctx.image_base=reinterpret_cast<uint64_t>(g_image.base());
      ctx.x[0]=arc_jni_env();ctx.x[1]=activity;int slot=2;for(auto a:args)ctx.x[slot++]=a;
      arc_s_w(&ctx,0,x);arc_s_w(&ctx,1,y);
      EntryCall call{};call_guest(fn,&call);
      if(call.rc){printf("[bsm] %s failed: %s %s\n",name,call.trap,FaultWhat(call.code).c_str());ReportFrames();ReportRegisters(&ctx);return false;}
      if(strcmp(name,"nativeTick"))printf("[bsm] returned %s\n",name);
      return true;
    };
    if(!invoke("nativeLoad",{activity,arc_jni_object()}))return 1;
    if(!invoke("nativeSurfaceCreated",{arc_jni_object(),uint64_t(window.width()),uint64_t(window.height())}))return 1;
    invoke("nativeResize",{uint64_t(window.width()),uint64_t(window.height())});
    invoke("nativeOrientationChanged",{0});
    invoke("nativeResume",{});
    invoke("nativeGainedAudioFocus",{});
    const std::pair<const char*,std::vector<uint64_t>> callbacks[]={
      {"nativeLicenseResult",{0,0}},
      {"nativeConsentReadySuccess",{}},
      {"nativeTCFString",{arc_jni_string("")}},
      {"nativeConsentCollectedSuccess",{}},
      {"_native_initCallback",{1}}, {"_native_itemCallback",{arc_jni_object()}},
      {"_native_completeRestoreCallback",{}},{"nativeOnRewardedVideoAvailabilityChanged",{0}},
      {"PlayServices_LoginCompleted",{1,arc_jni_string("")}}
    };
    bool sent[sizeof(callbacks)/sizeof(callbacks[0])]{};
    struct Tap{long frame;float x,y;};std::vector<Tap> taps;
    if(const char* script=getenv("BSM2_TAPS")){
      const char* at=script;while(*at){long f;float x,y;if(sscanf(at,"%ld:%f:%f",&f,&x,&y)==3)taps.push_back({f,x,y});const char* next=strchr(at,',');if(!next)break;at=next+1;}
    }
    const long capture_every=getenv("BSM2_CAPTURE_EVERY")?strtol(getenv("BSM2_CAPTURE_EVERY"),nullptr,10):0;
    bool alive=true;long frame=0;auto report_at=std::chrono::steady_clock::now();double tick_ms=0;auto deadline=report_at;long input_offset=0;
    while(alive&&window.PumpEvents()){
      if(window.TakeResized()){
        BsmSetDimensions(window.width(),window.height());
        alive=invoke("nativeResize",{uint64_t(window.width()),uint64_t(window.height())});
      }
      BsmCursorPoll(window.native_window());
      bool quit_requested=false;
      if(const char* path=getenv("BSM2_INPUT_FILE"))if(FILE* commands=fopen(path,"rb")){
        fseek(commands,input_offset,SEEK_SET);char line[1200];
        while(fgets(line,sizeof(line),commands)){
          if(!strchr(line,'\n'))break;
          input_offset=ftell(commands);float x,y;char shot_path[1100];
          if(sscanf(line,"tap %f %f",&x,&y)==2)taps.push_back({frame,x,y});
          else if(sscanf(line,"hover %f %f",&x,&y)==2)BsmCursorDiagnostic(x,y);
          else if(sscanf(line,"down %f %f",&x,&y)==2)alive=invoke("nativeTouchStarted",{0},x,y);
          else if(sscanf(line,"move %f %f",&x,&y)==2)alive=invoke("nativeTouchHeld",{0,1},x,y);
          else if(sscanf(line,"up %f %f",&x,&y)==2)alive=invoke("nativeTouchEnded",{0},x,y);
          else if(sscanf(line,"shot %1099[^\r\n]",shot_path)==1)BsmCaptureNext(shot_path);
          else if(!strncmp(line,"quit",4))quit_requested=true;
          printf("[bsm command] frame %ld %s",frame,line);
        }
        fclose(commands);
      }
      if(quit_requested)break;
      arc::Window::Pointer p;
      while(alive&&window.NextPointer(&p)){
        const char* n=p.kind==arc::Window::Pointer::Down?"nativeTouchStarted":p.kind==arc::Window::Pointer::Up?"nativeTouchEnded":"nativeTouchHeld";
        alive=invoke(n,{0,1},float(p.x),float(p.y));
      }
      for(auto& tap:taps){
        if(frame==tap.frame){printf("[bsm input] down %.1f %.1f\n",tap.x,tap.y);alive=invoke("nativeTouchStarted",{0},tap.x,tap.y);}
        if(frame==tap.frame+1)alive=invoke("nativeTouchHeld",{0,1},tap.x,tap.y);
        if(frame==tap.frame+2)alive=invoke("nativeTouchEnded",{0},tap.x,tap.y);
      }
      for(size_t i=0;alive&&i<sizeof(sent)/sizeof(sent[0]);++i){
        if(!sent[i]&&frame>long(10+i*8)&&BsmNative(callbacks[i].first)){
          sent[i]=true;auto& a=callbacks[i].second;
          if(a.empty())alive=invoke(callbacks[i].first,{});
          else if(a.size()==1)alive=invoke(callbacks[i].first,{a[0]});
          else alive=invoke(callbacks[i].first,{a[0],a[1]});
        }
      }
      if(shot&&frame+1==(frame_limit?frame_limit:1))BsmCaptureNext(shot);
      if(capture_every&&(frame+1)%capture_every==0){char name[80];snprintf(name,sizeof(name),"frame_%06ld.ppm",frame+1);BsmCaptureNext(name);}
      const auto tick_start=std::chrono::steady_clock::now();
      if(alive)alive=invoke("nativeTick",{});
      tick_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-tick_start).count();
      if(alive)BsmAudioPump();
      if(alive)BsmMusicPump();
      if(!alive)break;
      ++frame;g_frames_done.store(frame);
      if(frame%60==0){auto now=std::chrono::steady_clock::now();double seconds=std::chrono::duration<double>(now-report_at).count();printf("[bsm perf] frame %ld fps %.2f nativeTick %.2f ms callbacks %zu\n",frame,60/seconds,tick_ms/60,BsmNativeCount());report_at=now;tick_ms=0;}
      if(frame_limit&&frame>=frame_limit)break;
      if(getenv("BSM2_BASELINE_PACING"))std::this_thread::sleep_for(std::chrono::milliseconds(16));
      else{deadline+=std::chrono::microseconds(16667);auto now=std::chrono::steady_clock::now();if(deadline>now)std::this_thread::sleep_until(deadline);else deadline=now;}
    }
    invoke("nativePause",{});
    printf("[bsm] completed %ld frames, status %s\n",frame,alive?"OK":"FAULT");
    return alive?0:1;
  }

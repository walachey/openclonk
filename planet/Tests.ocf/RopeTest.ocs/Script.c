/**
	Rope Test
	Test scenario for the rope and all related objects.
	
	@author Maikel
*/


public func Initialize()
{
	// Rope ladders at the start.
	CreateObjectAbove(Ropeladder, 104, 40)->Unroll(1, 0, 12);
	CreateObjectAbove(Ropeladder, 222, 40)->Unroll(1, -1, 12);
	
	// Ropebridge.
	Ropebridge->Create(80, 152, 176, 152);
	
	// Series of rope ladders.
	CreateObjectAbove(Ropeladder, 292, 60)->Unroll(-1, COMD_Up);
	CreateObjectAbove(Ropeladder, 332, 60)->Unroll(-1, COMD_Up);
	CreateObjectAbove(Ropeladder, 372, 60)->Unroll(-1, COMD_Up);
	CreateObjectAbove(Ropeladder, 412, 60)->Unroll(-1, COMD_Up);
	CreateObjectAbove(Ropeladder, 452, 60)->Unroll(-1, COMD_Up);
	CreateObjectAbove(Ropeladder, 492, 60)->Unroll(-1, COMD_Up);
	
	DrawMaterialQuad("Earth", 550, 224, 600, 156, 640, 156, 640, 224);
	CreateObjectAbove(Ropeladder, 602, 158)->Unroll(1, -1);
	
	// Profile.
	ProfileObject(Ropeladder);
	return;
}

public func InitializePlayer(int plr)
{
	GetCrew(plr)->CreateContents(GrappleBow, 2);
	GetCrew(plr)->CreateContents(Ropeladder);
	GetCrew(plr)->CreateContents(Shovel);
	return;
}

global func ProfileObject(id def, int duration)
{
	if (duration == nil)
		duration = 10;
	StartScriptProfiler(def);
	ScheduleCall(nil, "StopScriptProfiler", duration * 36, 0);
}

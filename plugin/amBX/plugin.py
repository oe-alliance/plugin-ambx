import time
import os
from random import random
import enigma

# Config
from Components.config import config, ConfigEnableDisable, ConfigSubsection, \
			 ConfigYesNo, ConfigClock, getConfigListEntry, \
			 ConfigSelection, ConfigNumber
from Screens.Screen import Screen
from Components.ConfigList import ConfigListScreen
from Components.ActionMap import ActionMap
from Components.Button import Button
from Components.Label import Label
from Components.SelectionList import SelectionList, SelectionEntryComponent
import Components.PluginComponent

#Set default configuration
config.plugins.ambx = ConfigSubsection()
config.plugins.ambx.showinextensions = ConfigYesNo(default = True)
config.plugins.ambx.mode = ConfigSelection(default = "off", choices = [
		("off", _("Off")),
		("video", _("Video")),
		("color", _("Color")), 
		])
config.plugins.ambx.fader = ConfigSelection(default = '250', choices = [
		('0', _("Off")),
		('64', _("64 ms")),
		('125', _("125 ms")),
		('250', _("250 ms")),
		('500', _("500 ms")),
		])
config.plugins.ambx.color = ConfigSelection(default = '404040', choices = [
		('202020', _("Dark")),
		('404040', _("Medium")),
		('808080', _("Bright")),
		('FFFFFF', _("White")),
		('806030', _("Sunny")), 
		('408040', _("Park")),
		('405080', _("Ocean")), 
		])
# Plugin definition
from Plugins.Plugin import PluginDescriptor

# Global variable
_session = None

class Effects:
	def __init__(self):
		self.pyambx = None
		self.running = False
		self.grabbing = False
		self.mode = 'off'
		self.color = 0
		self.fader = 250
	def init(self):
		if self.pyambx is None:
			import pyambx
			self.pyambx = pyambx
	def start(self):
		if not self.running:
			self.init()
			self.pyambx.startOutput()
			self.running = True
	def stop(self):
		if self.running:
			if self.grabbing:
				self.grabbing = False
				self.pyambx.stopGrabber()
			self.pyambx.stopOutput()
			self.running = False
	def changeMode(self, mode):
		if mode == self.mode:
			return
		self.applyMode(mode)
	def applyMode(self, mode):
		if mode == 'off':
			self.stop()
		else:
			self.start()
		if mode == 'video':
			if not self.grabbing:
				self.grabbing = True
				self.pyambx.startGrabber()
		elif mode == 'color':
			if self.grabbing:
				self.grabbing = False
				self.pyambx.stopGrabber()
			self.pyambx.setLight(self.color) 
		self.mode = mode
	def changeColor(self, color):
		if self.pyambx is not None:
			self.pyambx.setLight(color) 
		self.color = color
	def changeFader(self, speed):
		self.fader = speed
		if self.pyambx is not None:
			self.pyambx.setFadeTime(speed)
	def enterStandby(self):
		self.stop()
	def leaveStandby(self):
		self.applyMode(self.mode)
		if self.pyambx is not None:
			self.pyambx.setLight(self.color) 
	def __del__(self):
		self.stop()

effects = Effects()

##################################
# Configuration GUI

class Config(ConfigListScreen,Screen):
	skin = """
<screen position="center,center" size="560,400" title="amBX Configuration" >
	<ePixmap name="red"    position="0,0"   zPosition="2" size="140,40" pixmap="skin_default/buttons/red.png" transparent="1" alphatest="on" />
	<ePixmap name="green"  position="140,0" zPosition="2" size="140,40" pixmap="skin_default/buttons/green.png" transparent="1" alphatest="on" />
	<ePixmap name="yellow" position="280,0" zPosition="2" size="140,40" pixmap="skin_default/buttons/yellow.png" transparent="1" alphatest="on" /> 
	<ePixmap name="blue"   position="420,0" zPosition="2" size="140,40" pixmap="skin_default/buttons/blue.png" transparent="1" alphatest="on" /> 

	<widget name="key_red" position="0,0" size="140,40" valign="center" halign="center" zPosition="4"  foregroundColor="white" font="Regular;20" transparent="1" shadowColor="background" shadowOffset="-2,-2" /> 
	<widget name="key_green" position="140,0" size="140,40" valign="center" halign="center" zPosition="4"  foregroundColor="white" font="Regular;20" transparent="1" shadowColor="background" shadowOffset="-2,-2" /> 
	<widget name="key_yellow" position="280,0" size="140,40" valign="center" halign="center" zPosition="4"  foregroundColor="white" font="Regular;20" transparent="1" shadowColor="background" shadowOffset="-2,-2" />
	<widget name="key_blue" position="420,0" size="140,40" valign="center" halign="center" zPosition="4"  foregroundColor="white" font="Regular;20" transparent="1" shadowColor="background" shadowOffset="-2,-2" />

	<widget name="config" position="10,40" size="540,240" scrollbarMode="showOnDemand" />

	<ePixmap alphatest="on" pixmap="skin_default/icons/clock.png" position="480,383" size="14,14" zPosition="3"/>
	<widget font="Regular;18" halign="left" position="505,380" render="Label" size="55,20" source="global.CurrentTime" transparent="1" valign="center" zPosition="3">
		<convert type="ClockToText">Default</convert>
	</widget>
	<widget name="statusbar" position="10,380" size="470,20" font="Regular;18" />
	<widget name="status" position="10,300" size="540,60" font="Regular;20" />
</screen>"""
		
	def __init__(self, session, args = 0):
		self.session = session
		self.setup_title = _("amBX Configuration")
		Screen.__init__(self, session)
		cfg = config.plugins.ambx
		self.list = [
			getConfigListEntry(_("Effect mode"), cfg.mode),
			getConfigListEntry(_("Show in extensions"), cfg.showinextensions),
			getConfigListEntry(_("Fader speed"), cfg.fader),
			getConfigListEntry(_("Color"), cfg.color),
			]
		ConfigListScreen.__init__(self, self.list, session = self.session, on_change = self.changedEntry)
		self["status"] = Label()
		self["statusbar"] = Label()
		self["key_red"] = Button(_("Cancel"))
		self["key_green"] = Button(_("Ok"))
		self["key_yellow"] = Button()
		self["key_blue"] = Button()
		self["setupActions"] = ActionMap(["SetupActions", "ColorActions"],
		{
			"red": self.cancel,
			"green": self.save,
			"save": self.save,
			"cancel": self.cancel,
			"ok": self.save,
		}, -2)
		self.onChangedEntry = []
		self.updateTimer = enigma.eTimer()
	    	self.updateTimer.callback.append(self.updateStatus)
		self.updateTimer.start(2000)
		self.updateStatus()
	
	# for summary:
	def changedEntry(self):
		for x in self.onChangedEntry:
			x()
	def getCurrentEntry(self):
		return self["config"].getCurrent()[0]
	def getCurrentValue(self):
		return str(self["config"].getCurrent()[1].getText())
	def createSummary(self):
		from Screens.Setup import SetupSummary
		return SetupSummary

	def save(self):
		#print "saving"
		self.updateTimer.stop()
		self.saveAll()
		self.close(True,self.session)

	def cancel(self):
		#print "cancel"
		self.updateTimer.stop()
		for x in self["config"].list:
			x[1].cancel()
		self.close(False,self.session)

	def updateStatus(self):
		if not effects.running:
			text = _("not running")
		elif effects.grabbing:
			frames, usec = effects.pyambx.getFpsStats()
			if usec:
				fps = "%.3g" % (frames*1000000.0/usec)
			else:
				fps = "--"   
			text = _("Grabber FPS") + ": " + fps 
				
		else:
			text = _("Color mode")
		self["status"].setText(text)
		
def main(session, **kwargs):
    session.open(Config)


##################################
# Autostart section

def autostart(reason, session=None, **kwargs):
	"called with reason=1 to during shutdown, with reason=0 at startup"
	global _session
	if reason == 0:
		config.plugins.ambx.mode.addNotifier(changeMode)
		config.plugins.ambx.color.addNotifier(changeColor)
		config.plugins.ambx.fader.addNotifier(changeFader)
		config.misc.standbyCounter.addNotifier(standbyCounterChanged, initial_call = False)
    		if session is not None:
			_session = session
    	else:
    		try:
    			effects.stop()
    		except:
    			pass

# we need this helper function to identify the descriptor
def extensionsmenu(session, **kwargs):
	main(session, **kwargs)

def housekeepingExtensionsmenu(el):
	try:
		if el.value:
			Components.PluginComponent.plugins.addPlugin(extDescriptor)
		else:
			Components.PluginComponent.plugins.removePlugin(extDescriptor)
	except Exception, e:
		print "[amBX] Failed to update extensions menu:", e

def changeMode(el):
	global effects
	try:
		effects.changeMode(el.value)
	except Exception, e:
		print "[amBX] Failed to start/stop effects:", e

def changeColor(el):
	global effects
	try:
		effects.changeColor(int(el.value, 16))
	except Exception, e:
		print "[amBX] Failed to switch color:", e

def changeFader(el):
	global effects
	try:
		effects.changeFader(int(el.value))
	except Exception, e:
		print "[amBX] Failed to switch fader:", e

def leaveStandby():
	global effects
	try:
		effects.leaveStandby()
	except Exception, e:
		print "[amBX] Failed to enable on standby:", e

def standbyCounterChanged(configElement):
	print "[amBX] Going to standby"
	global effects
	try:
		import Screens.Standby
		effects.enterStandby()
		if leaveStandby not in Screens.Standby.inStandby.onClose:
			Screens.Standby.inStandby.onClose.append(leaveStandby)
	except Exception, e:
		print "[amBX] Failed to disable on standby:", e
	

description = _("amBX Effects")
config.plugins.ambx.showinextensions.addNotifier(housekeepingExtensionsmenu, initial_call = False, immediate_feedback = False)
extDescriptor = PluginDescriptor(name="amBX", description = description, where = PluginDescriptor.WHERE_EXTENSIONSMENU, fnc = extensionsmenu)

def Plugins(**kwargs):
    result = [
        PluginDescriptor(
            name="amBX",
            description = description,
            where = [
                PluginDescriptor.WHERE_AUTOSTART,
                PluginDescriptor.WHERE_SESSIONSTART
            ],
            fnc = autostart
        ),
    
        PluginDescriptor(
            name="amBX",
            description = description,
            where = PluginDescriptor.WHERE_PLUGINMENU,
            icon = 'plugin.png',
            fnc = main
        ),
    ]
    if config.plugins.ambx.showinextensions.value:
    	result.append(extDescriptor)
    return result

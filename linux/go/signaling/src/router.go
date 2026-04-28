package main

import (
	"signaling/src/action"
	"signaling/src/framework"
)

func init() {
	framework.GActionRouter["/signaling/push"] = action.NewPushAction()
	framework.GActionRouter["/signaling/stoppush"] = action.NewStopPushAction()
	framework.GActionRouter["/signaling/pull"] = action.NewPullAction()
	framework.GActionRouter["/signaling/stoppull"] = action.NewStopPullAction()
	framework.GActionRouter["/signaling/sendanswer"] = action.NewSendAnswerAction()

	framework.GActionRouter["/xrtcclient/push"] = action.NewXrtcClientPushAction()
	framework.GActionRouter["/xrtcclient/pull"] = action.NewXrtcClientPullAction()
}

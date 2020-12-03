/*
 * purple - Jabber Protocol Plugin
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include <glib/gi18n-lib.h>

#include <purple.h>

#include "usermood.h"
#include "pep.h"
#include <string.h>

static PurpleMood moods[] = {
	{"afraid", N_("Afraid"), },
	{"amazed", N_("Amazed"), },
	{"amorous", N_("Amorous"), },
	{"angry", N_("Angry"), },
	{"annoyed", N_("Annoyed"), },
	{"anxious", N_("Anxious"), },
	{"aroused", N_("Aroused"), },
	{"ashamed", N_("Ashamed"), },
	{"bored", N_("Bored"), },
	{"brave", N_("Brave"), },
	{"calm", N_("Calm"), },
	{"cautious", N_("Cautious"), },
	{"cold", N_("Cold"), },
	{"confident", N_("Confident"), },
	{"confused", N_("Confused"), },
	{"contemplative", N_("Contemplative"), },
	{"contented", N_("Contented"), },
	{"cranky", N_("Cranky"), },
	{"crazy", N_("Crazy"), },
	{"creative", N_("Creative"), },
	{"curious", N_("Curious"), },
	{"dejected", N_("Dejected"), },
	{"depressed", N_("Depressed"), },
	{"disappointed", N_("Disappointed"), },
	{"disgusted", N_("Disgusted"), },
	{"dismayed", N_("Dismayed"), },
	{"distracted", N_("Distracted"), },
	{"embarrassed", N_("Embarrassed"), },
	{"envious", N_("Envious"), },
	{"excited", N_("Excited"), },
	{"flirtatious", N_("Flirtatious"), },
	{"frustrated", N_("Frustrated"), },
	{"grateful", N_("Grateful"), },
	{"grieving", N_("Grieving"), },
	{"grumpy", N_("Grumpy"), },
	{"guilty", N_("Guilty"), },
	{"happy", N_("Happy"), },
	{"hopeful", N_("Hopeful"), },
	{"hot", N_("Hot"), },
	{"humbled", N_("Humbled"), },
	{"humiliated", N_("Humiliated"), },
	{"hungry", N_("Hungry"), },
	{"hurt", N_("Hurt"), },
	{"impressed", N_("Impressed"), },
	{"in_awe", N_("In awe"), },
	{"in_love", N_("In love"), },
	{"indignant", N_("Indignant"), },
	{"interested", N_("Interested"), },
	{"intoxicated", N_("Intoxicated"), },
	{"invincible", N_("Invincible"), },
	{"jealous", N_("Jealous"), },
	{"lonely", N_("Lonely"), },
	{"lost", N_("Lost"), },
	{"lucky", N_("Lucky"), },
	{"mean", N_("Mean"), },
	{"moody", N_("Moody"), },
	{"nervous", N_("Nervous"), },
	{"neutral", N_("Neutral"), },
	{"offended", N_("Offended"), },
	{"outraged", N_("Outraged"), },
	{"playful", N_("Playful"), },
	{"proud", N_("Proud"), },
	{"relaxed", N_("Relaxed"), },
	{"relieved", N_("Relieved"), },
	{"remorseful", N_("Remorseful"), },
	{"restless", N_("Restless"), },
	{"sad", N_("Sad"), },
	{"sarcastic", N_("Sarcastic"), },
	{"satisfied", N_("Satisfied"), },
	{"serious", N_("Serious"), },
	{"shocked", N_("Shocked"), },
	{"shy", N_("Shy"), },
	{"sick", N_("Sick"), },
	{"sleepy", N_("Sleepy"), },
	{"spontaneous", N_("Spontaneous"), },
	{"stressed", N_("Stressed"), },
	{"strong", N_("Strong"), },
	{"surprised", N_("Surprised"), },
	{"thankful", N_("Thankful"), },
	{"thirsty", N_("Thirsty"), },
	{"tired", N_("Tired"), },
	{"undefined", N_("Undefined"), },
	{"weak", N_("Weak"), },
	{"worried", N_("Worried"), },
	/* Mark last record. */
	{NULL, }
};

static const PurpleMood*
find_mood_by_name(const gchar *name)
{
	int i;

	g_return_val_if_fail(name && *name, NULL);

	for (i = 0; moods[i].mood != NULL; ++i) {
		if (purple_strequal(name, moods[i].mood)) {
			return &moods[i];
		}
	}

	return NULL;
}

static void jabber_mood_cb(JabberStream *js, const char *from, PurpleXmlNode *items) {
	/* it doesn't make sense to have more than one item here, so let's just pick the first one */
	PurpleXmlNode *item = purple_xmlnode_get_child(items, "item");
	const char *newmood = NULL;
	char *moodtext = NULL;
	JabberBuddy *buddy = jabber_buddy_find(js, from, FALSE);
	PurpleXmlNode *moodinfo, *mood;
	/* ignore the mood of people not on our buddy list */
	if (!buddy || !item)
		return;

	mood = purple_xmlnode_get_child_with_namespace(item, "mood", "http://jabber.org/protocol/mood");
	if (!mood)
		return;
	for (moodinfo = mood->child; moodinfo; moodinfo = moodinfo->next) {
		if (moodinfo->type == PURPLE_XMLNODE_TYPE_TAG) {
			if (purple_strequal(moodinfo->name, "text")) {
				if (!moodtext) /* only pick the first one */
					moodtext = purple_xmlnode_get_data(moodinfo);
			} else {
				const PurpleMood *target_mood;

				/* verify that the mood is known (valid) */
				target_mood = find_mood_by_name(moodinfo->name);
				newmood = target_mood ? target_mood->mood : NULL;
			}

		}
		if (newmood != NULL && moodtext != NULL)
			break;
	}
	if (newmood != NULL) {
		purple_protocol_got_user_status(purple_connection_get_account(js->gc), from, "mood",
				PURPLE_MOOD_NAME, newmood,
				PURPLE_MOOD_COMMENT, moodtext,
				NULL);
	} else {
		purple_protocol_got_user_status_deactive(purple_connection_get_account(js->gc), from, "mood");
	}
	g_free(moodtext);
}

void jabber_mood_init(void) {
	jabber_add_feature("http://jabber.org/protocol/mood", jabber_pep_namespace_only_when_pep_enabled_cb);
	jabber_pep_register_handler("http://jabber.org/protocol/mood", jabber_mood_cb);
}

gboolean
jabber_mood_set(JabberStream *js, const char *mood, const char *text)
{
	const PurpleMood *target_mood = NULL;
	PurpleXmlNode *publish, *moodnode;

	if (mood && *mood) {
		target_mood = find_mood_by_name(mood);
		/* Mood specified, but is invalid --
		 * fail so that the command can handle this.
		 */
		if (!target_mood)
			return FALSE;
	}

	publish = purple_xmlnode_new("publish");
	purple_xmlnode_set_attrib(publish,"node","http://jabber.org/protocol/mood");
	moodnode = purple_xmlnode_new_child(purple_xmlnode_new_child(publish, "item"), "mood");
	purple_xmlnode_set_namespace(moodnode, "http://jabber.org/protocol/mood");

	if (target_mood) {
		/* If target_mood is not NULL, then
		 * target_mood->mood == mood, and is a valid element name.
		 */
	    purple_xmlnode_new_child(moodnode, mood);

		/* Only set text when setting a mood */
		if (text && *text) {
			PurpleXmlNode *textnode = purple_xmlnode_new_child(moodnode, "text");
			purple_xmlnode_insert_data(textnode, text, -1);
		}
	}

	jabber_pep_publish(js, publish);
	return TRUE;
}

PurpleMood *
jabber_get_moods(PurpleProtocolClient *client, PurpleAccount *account) {
	return moods;
}

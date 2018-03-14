#define NODES_MAX 128
#define LINES_MAX 128
#define NODE_NAME_MAX SHORT_STR

#define LINES_LAYER 40
#define NODES_LAYER 50

namespace Writer {

	void initGraph();
	void submitNode(const char *name, const char *passage);
	void attachNode(const char *prev, const char *next, const char *dirStr);

	void showGraph();
	void updateGraph();
	void hideGraph();

	void setNodeLocked(char *nodeName, char *varName);

	struct Node {
		char name[NODE_NAME_MAX];
		char passage[PASSAGE_NAME_MAX];
		char *unlockVarName;
		MintSprite *sprite;

		Node *connectedTo;
		Dir8 connectedDir;
	};

	struct GraphStruct {
		float creationTime;
		MintSprite *bg;

		Node nodes[NODES_MAX];
		int nodesNum;

		MintSprite *lines[LINES_MAX];
		int linesNum;

		MintSprite *saveButton;
		MintSprite *loadButton;
	};

	GraphStruct *graph;

	void initGraph() {
		graph = (GraphStruct *)zalloc(sizeof(GraphStruct));
	}

	void submitNode(const char *name, const char *passage) {
		Node *node = &graph->nodes[graph->nodesNum++];
		strcpy(node->name, name);
		strcpy(node->passage, passage);
	}

	void showGraph() {
		graph->linesNum = 0;
		graph->creationTime = engine->time;

		{ /// Bg
			MintSprite *spr = createMintSprite();
			spr->setupRect(2048, 2048, 0x222222);
			graph->bg = spr;
		}

		for (int i = 0; i < graph->nodesNum; i++) {
			Node *node = &graph->nodes[i];

			{ /// Node sprite
				MintSprite *spr = createMintSprite();
				spr->setup9Slice("img/writer/nodeChoice", 180, 180, 22, 22, 66, 66);
				graph->bg->addChild(spr);
				spr->layer = lowestLayer + NODES_LAYER;

				node->sprite = spr;
			}

			{ /// Node text
				MintSprite *spr = createMintSprite();
				spr->setupEmpty(node->sprite->width, node->sprite->height);
				node->sprite->addChild(spr);
				spr->setText(node->name);
				spr->layer = lowestLayer + NODES_LAYER;
				spr->alignOutside(DIR8_CENTER);
			}

			{ /// Node locked
				if (node->unlockVarName) {
					execJs("var nodeLockedStr = %s ? \"1\" : \"0\";", node->unlockVarName);

					mjs_val_t jsData = mjs_get(mjs, mjs_get_global(mjs), "nodeLockedStr", strlen("nodeLockedStr"));
					size_t jsStrLen = 0;
					const char *jsStr = mjs_get_string(mjs, &jsData, &jsStrLen);
					bool isUnlocked = jsStr[0] == '1' ? true : false;
					// printf("%s is %d\n", node->name, isUnlocked);

					execJs("var nodeLockedStr = null;");

					if (!isUnlocked) node->sprite->alpha = 0.5;
				}
			}
		}

		for (int i = 0; i < graph->nodesNum; i++) {
			Node *node = &graph->nodes[i];
			Node *other = node->connectedTo;
			if (other) {
				node->sprite->alignOutside(other->sprite, node->connectedDir, 50, 50);

				{ /// Line
					Point p1 = node->sprite->getCenterPoint();
					Point p2 = other->sprite->getCenterPoint();
					float dist = pointDistance(&p1, &p2);

					MintSprite *spr = createMintSprite();
					spr->setupRect(dist, 4, 0xFFFFFF);
					spr->centerPivot = true;
					spr->layer = lowestLayer + LINES_LAYER;
					spr->x = (p1.x + p2.x) / 2.0 - spr->width/2;
					spr->y = (p1.y + p2.y) / 2.0 - spr->height/2;
					spr->rotation = atan2(p2.y - p1.y, p2.x - p1.x) * 180 / M_PI;
					graph->bg->addChild(spr);

					graph->lines[graph->linesNum++] = spr;
				}
			}
		}

		{ /// Save button
			MintSprite *spr = createMintSprite("writer/exit.png");
			spr->scale(2, 2);
			spr->x = engine->width - spr->getWidth() - 16; //@hardcode padding 16px
			spr->y = 16; //@hardcode padding 16px

			graph->saveButton = spr;
		}

		{ /// load button
			MintSprite *spr = createMintSprite("writer/restart.png");
			writer->bg->addChild(spr);
			spr->scale(2, 2);
			spr->x = graph->saveButton->x;
			spr->y = graph->saveButton->y + graph->saveButton->getHeight() + 16; //@hardcode padding 16px

			graph->loadButton = spr;
		}

		mjs_val_t onMapStartFn = mjs_get(mjs, mjs_get_global(mjs), "onMapStart", strlen("onMapStart"));
		execJs(onMapStartFn);
	}

	void hideGraph() {
		graph->bg->destroy();
		graph->saveButton->destroy();
		graph->loadButton->destroy();

		for (int i = 0; i < graph->nodesNum; i++) {
			Node *node = &graph->nodes[i];
			if (node->sprite) {
				node->sprite = NULL;
			}
		}
	}

	void updateGraph() {
		if (graph->bg->holding && engine->time - graph->creationTime > 1) {
			graph->bg->x = engine->mouseX - graph->bg->holdPivot.x;
			graph->bg->y = engine->mouseY - graph->bg->holdPivot.y;

			graph->bg->x = Clamp(graph->bg->x, -(graph->bg->width - engine->width), 0);
			graph->bg->y = Clamp(graph->bg->y, -(graph->bg->height - engine->height), 0);
		}

		if (graph->saveButton->justPressed) saveGame();
		if (graph->loadButton->justPressed) loadGame();

		for (int i = 0; i < graph->nodesNum; i++) {
			Node *node = &graph->nodes[i];

			if (node->sprite && node->sprite->justPressed) {
				changeState(STATE_MOD);
				gotoPassage(node->passage);
			}
		}
	}

	void attachNode(const char *prev, const char *next, const char *dirStr) {
		Node *node1 = NULL;
		Node *node2 = NULL;

		Dir8 dir = DIR8_CENTER;
		if (streq(dirStr, LEFT)) dir = DIR8_LEFT;
		else if (streq(dirStr, RIGHT)) dir = DIR8_RIGHT;
		else if (streq(dirStr, TOP)) dir = DIR8_UP;
		else if (streq(dirStr, BOTTOM)) dir = DIR8_DOWN;

		for (int i = 0; i < graph->nodesNum; i++)
			if (streq(graph->nodes[i].name, next))
				node1 = &graph->nodes[i];

		for (int i = 0; i < graph->nodesNum; i++)
			if (streq(graph->nodes[i].name, prev))
				node2 = &graph->nodes[i];

		Assert(node1);
		Assert(node2);

		node1->connectedTo = node2;
		node1->connectedDir = dir;
	}

	void setNodeLocked(char *nodeName, char *varName) {
		Node *node = NULL;
		for (int i = 0; i < graph->nodesNum; i++)
			if (streq(graph->nodes[i].name, nodeName))
				node = &graph->nodes[i];

		if (!node) {
			msg("Failed to set lock because node named %s doen't exist", MSG_ERROR, nodeName);
			return;
		}

		node->unlockVarName = stringClone(varName);
	}
}

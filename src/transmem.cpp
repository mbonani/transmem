/*! \file   transmem.cpp
    \brief  source file transmem.cpp
*/

#include "transmem.h"

/****************************
 * NOSUCHLINKFOUNDEXCEPTION *
 ****************************/

/****************************
 * TRANSMEM                 *
 ****************************/

void TransMem::registerLink(const FrameID &srcFrame, const FrameID &destFrame, const Timestamp &tstamp, const QQuaternion &qrot, const QQuaternion &qtrans){

    // TODO: check if normalized rotation quaternion and pure translation quaternion?!

    // get the lock
    lock.lock();

    // check if frames already exist
    auto IterSrcF = frameID2Frame.find(srcFrame);
    auto IterDstF = frameID2Frame.find(destFrame);

    Frame* sFPtr; Frame* dFPtr;

    // if a frame does not exist, create it
    if(IterSrcF == frameID2Frame.end()){

        frames.emplace_back(Frame{srcFrame});
        sFPtr = &frames.back();
        frameID2Frame.insert({srcFrame, sFPtr});

    }
    else
        sFPtr = (*IterSrcF).second;

    if(IterDstF == frameID2Frame.end()){

        frames.emplace_back(Frame{destFrame});
        dFPtr = &frames.back();
        frameID2Frame.insert({destFrame, dFPtr});

    }
    else
        dFPtr = (*IterDstF).second;

    // check if the link exists
    Link* lnkPtr = nullptr;
    sFPtr->connectionTo(destFrame, lnkPtr);

    // if the link does not exist, create it
    if(lnkPtr == nullptr){

        links.emplace_back(Link{sFPtr, dFPtr, storageTime});
        lnkPtr = &links.back();
        sFPtr->addLink(lnkPtr);
    }

    // add the transformation to the link
    lnkPtr->addTransformation(srcFrame, StampedTransformation{tstamp, qrot, qtrans});

    // release the lock
    lock.unlock();

    return;
}

void TransMem::registerLink(const FrameID &srcFrame, const FrameID &destFrame, const Timestamp &tstamp, const QMatrix4x4 &trans) {

   float data[]{trans(0,0),trans(0,1),trans(0,2),trans(1,0),trans(1,1),trans(1,2),trans(2,0),trans(2,1),trans(2,2)};
   registerLink(srcFrame, destFrame, tstamp, QQuaternion::fromRotationMatrix(QMatrix3x3(data)), QQuaternion(0, trans(0,3), trans(1,3), trans(2,3)));

}

void TransMem::writeJSON(QJsonObject &json) const {

    QJsonArray frameObjects;
    for(Frame f: frames){
        QJsonObject frameObject;
        f.writeJSON(frameObject);
        frameObjects.append(frameObject);
    }

    QJsonArray linkObjects;
    for(Link l: links){
        QJsonObject linkObject;
        l.writeJSON(linkObject);
        linkObjects.append(linkObject);
    }

    json.insert("frames", frameObjects);
    json.insert("links", linkObjects);

}

void TransMem::shortestPath(Path &p){

    // check if the source frame exists
    auto iterS = frameID2Frame.find(p.src);
    if(iterS == frameID2Frame.end()){
        // TODO: error msg
        // no path exists for sure
        return;
    }

    // check if the dest frame exists
    auto iterD = frameID2Frame.find(p.dst);
    if(iterD == frameID2Frame.end()){
        // TODO: error msg
        // no path exists for sure
        return;
    }

    Frame* currFrame = (*iterS).second;

    auto cmp = [](Frame *f1, Frame *f2){return f1->distance > f2->distance;};
    std::priority_queue<Frame*, std::vector<Frame*>, decltype(cmp) > pq(cmp);

    // initialize for dikstra
    // set the distance of all frames to infinity and the predecessor to null
    for(Frame f: frames){
        f.distance = std::numeric_limits<double>::infinity();
        f.predecessor = nullptr;
        pq.push(&f);
    }
    // set the distance of the src frame to zero
        currFrame->distance = 0.;

    auto updateDistance = [](Frame* cu, Frame* ne){
        double alternativeDist = cu->distance + ne->distance;
        if(alternativeDist < ne->distance){
            ne->distance = alternativeDist;
            ne->predecessor = cu;
            return;
        }
    };

    // run dikstra
    currFrame = pq.top(); pq.pop(); currFrame->active = false;
    while(currFrame->frameID != p.src){
        for(Link* l: currFrame->parents)
            if(l->parent->active)
                updateDistance(currFrame, l->parent);
        for(Link* l: currFrame->children)
            if(l->child->active)
                updateDistance(currFrame, l->child);
    currFrame = pq.top(); pq.pop(); currFrame->active = false;
    }

    // create shortest path
    Link* currL;
    currFrame->connectionTo(currFrame->predecessor->frameID, currL); p.links.push_back(currL);
    while(currFrame->predecessor != nullptr){
        currFrame = currFrame->predecessor;
        currFrame->connectionTo(currFrame->predecessor->frameID, currL); p.links.push_back(currL);
    }

}

bool TransMem::dumpAsJSON(){

    QFile file("TransMemDump.json");
    if(!file.open(QIODevice::WriteOnly)){
        // TODO: error msg
        return false;
    }

    QJsonObject transmemObject; writeJSON(transmemObject);

    QJsonDocument saveJSON(transmemObject);
    file.write(saveJSON.toJson());

    file.close();
    if(file.error()){
        // TODO: error msg
        return false;
    }

    return true;
}

void TransMem::dumpAsGraphML(){

   GMLWriter writer;

   writer.write(this);

}

/****************************
 * PATH                     *
 ****************************/

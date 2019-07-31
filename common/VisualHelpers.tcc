
namespace SE {

namespace HELPERS {

void DrawAxes(const float size) {

  glLineWidth(4);

  glColor3f(1, 0, 0);
  glBegin(GL_LINES);
  glVertex3f(0, 0, 0);
  glVertex3f(size, 0, 0);
  glEnd();

  glColor3f(0, 1, 0);
  glBegin(GL_LINES);
  glVertex3f(0, 0, 0);
  glVertex3f(0, size, 0);
  glEnd();

  glColor3f(0, 0, 1);
  glBegin(GL_LINES);
  glVertex3f(0, 0, 0);
  glVertex3f(0, 0, size);
  glEnd();

  glLineWidth(1);

  glColor3f(1, 0, 0);
  glBegin(GL_LINES);
  for (float i = 0; i < size; ++i) {
    glVertex3f(0, i, 0);
    glVertex3f(size, i, 0);
  }
  glEnd();

  glColor3f(0, 1, 0);
  glBegin(GL_LINES);
  for (float i = 0; i < size; ++i) {
    glVertex3f(0, 0, i);
    glVertex3f(0, size, i);
  }
  glEnd();

  glColor3f(0, 0, 1);
  glBegin(GL_LINES);
  for (float i = 0; i < size; ++i) {
    glVertex3f(i, 0, 0);
    glVertex3f(i, 0, size);
  }
  glEnd();

}




Elipse::Elipse(const float x_pos, const float y_pos, const float z_pos, const float new_radius, const uint32_t new_edge_count) : radius(new_radius), edge_count(new_edge_count) {

  delta = (360 * (M_PI / 180.0)) / edge_count;

  ChangePos(x_pos, y_pos, z_pos);
  Resize(new_radius, new_edge_count);
}

Elipse::~Elipse() throw() { ;; }

void Elipse::Resize(const float new_radius, const uint32_t new_edge_count) {

  radius      = new_radius;
  edge_count  = new_edge_count;

  oVertArray.resize(edge_count);
  oColorArray.resize(edge_count);

  float alpha;

  for (uint32_t i = 0; i < edge_count; ++i) {
    alpha = delta * i;
    oVertArray[i].first     = radius * cos(alpha);  //x
    oVertArray[i].second    = radius * sin(alpha);  //y

    oColorArray[i].first    = (oVertArray[i].first  > 0) ? oVertArray[i].first  / radius : fabs(oVertArray[i].first) / (radius *  2);
    oColorArray[i].second   = (oVertArray[i].second > 0) ? oVertArray[i].second / radius : fabs(oVertArray[i].second) / (radius * 2);

    //log_d("oColorArray: x = {}, y = {}", oColorArray[i].first, oColorArray[i].second);
  }

}

void Elipse::ChangePos(const float x_pos, const float y_pos, const float z_pos) {

  pos[0] = x_pos;
  pos[1] = y_pos;
  pos[2] = z_pos;
}

void Elipse::Draw() const {

  glBegin(GL_LINES);
  for (uint32_t i = 1; i < edge_count; ++i) {

    glColor3f(oColorArray[i - 1].first, oColorArray[i - 1].second, 0);
    glVertex3f(oVertArray[i - 1].first + pos[0], oVertArray[i - 1].second + pos[1], pos[2]);

    glColor3f(oColorArray[i].first, oColorArray[i].second, 0);
    glVertex3f(oVertArray[i].first + pos[0], oVertArray[i].second + pos[1], pos[2]);
  }

  glColor3f(oColorArray[edge_count - 1].first, oColorArray[edge_count - 1].second, 0);
  glVertex3f(oVertArray[edge_count - 1].first + pos[0], oVertArray[edge_count - 1].second + pos[1], pos[2]);

  glColor3f(oColorArray[0].first, oColorArray[0].second, 0);
  glVertex3f(oVertArray[0].first + pos[0], oVertArray[0].second + pos[1], pos[2]);

  glEnd();

}


void DrawPlane(const float width, const float height,
               const float x_pos, const float y_pos, const float z_pos,
               const float x_dir, const float y_dir, const float z_dir,
               const uint32_t texture_id) {

        //CCW order

        glColor3f(1, 1, 1);
        glBindTexture (GL_TEXTURE_2D, texture_id);
        glBegin(GL_QUADS);

        glTexCoord2f (0, 0);
        //glNormal3f(0, 1, 0);
        glVertex3f(x_pos, y_pos, z_pos);

        glTexCoord2f (1, 0);
        //glNormal3f(0, 1, 0);
        glVertex3f(x_dir * width + x_pos, y_pos, z_pos);

        glTexCoord2f (1, 1);
        //glNormal3f(0, 1, 0);
        glVertex3f(x_dir * width + x_pos, y_pos, z_dir * height + z_pos);

        glTexCoord2f (0, 1);
        //glNormal3f(0, 1, 0);
        glVertex3f(x_pos, y_pos, z_dir * height + z_pos);

        glEnd();
}


void DrawBox(const float x_size,
             const float y_size,
             const float z_size,
             const float x_pos,
             const float y_pos,
             const float z_pos,
             const uint32_t texture_id) {

        glColor3f(1, 1, 1);
        glBindTexture (GL_TEXTURE_2D, texture_id);

        glBegin(GL_QUADS);
        glTexCoord2f (0, 0);
        glVertex3f(x_pos, y_pos, z_pos);

        glTexCoord2f (1, 0);
        glVertex3f(x_size + x_pos, y_pos, z_pos);

        glTexCoord2f (1, 1);
        glVertex3f(x_size + x_pos, y_size + y_pos, z_pos);

        glTexCoord2f (0, 1);
        glVertex3f(x_pos, y_size + y_pos, z_pos);
        glEnd();

        glBegin(GL_QUADS);
        glTexCoord2f (0, 0);
        glVertex3f(x_pos, y_pos, z_pos + z_size);

        glTexCoord2f (1, 0);
        glVertex3f(x_size + x_pos, y_pos, z_pos + z_size);

        glTexCoord2f (1, 1);
        glVertex3f(x_size + x_pos, y_size + y_pos, z_pos + z_size);

        glTexCoord2f (0, 1);
        glVertex3f(x_pos, y_size + y_pos, z_pos + z_size);
        glEnd();


        glBegin(GL_QUADS);
        glTexCoord2f (0, 0);
        glVertex3f(x_pos, y_pos, z_pos);

        glTexCoord2f (1, 0);
        glVertex3f(x_pos , y_pos, z_pos + z_size);

        glTexCoord2f (1, 1);
        glVertex3f(x_pos, y_size + y_pos, z_pos + z_size);

        glTexCoord2f (0, 1);
        glVertex3f(x_pos, y_size + y_pos, z_pos);
        glEnd();


        glBegin(GL_QUADS);
        glTexCoord2f (0, 0);
        glVertex3f(x_pos + x_size, y_pos, z_pos);

        glTexCoord2f (1, 0);
        glVertex3f(x_pos + x_size , y_pos, z_pos + z_size);

        glTexCoord2f (1, 1);
        glVertex3f(x_pos + x_size, y_size + y_pos, z_pos + z_size);

        glTexCoord2f (0, 1);
        glVertex3f(x_pos + x_size, y_size + y_pos, z_pos);
        glEnd();


        glBegin(GL_QUADS);
        glTexCoord2f (0, 0);
        glVertex3f(x_pos, y_pos, z_pos);

        glTexCoord2f (1, 0);
        glVertex3f(x_pos , y_pos, z_pos + z_size);

        glTexCoord2f (1, 1);
        glVertex3f(x_pos + x_size, y_pos, z_pos + z_size);

        glTexCoord2f (0, 1);
        glVertex3f(x_pos + x_size, y_pos, z_pos);
        glEnd();

        glBegin(GL_QUADS);
        glTexCoord2f (0, 0);
        glVertex3f(x_pos, y_pos + y_size, z_pos);

        glTexCoord2f (1, 0);
        glVertex3f(x_pos , y_pos + y_size, z_pos + z_size);

        glTexCoord2f (1, 1);
        glVertex3f(x_pos + x_size, y_pos + y_size, z_pos + z_size);

        glTexCoord2f (0, 1);
        glVertex3f(x_pos + x_size, y_pos + y_size, z_pos);
        glEnd();

}


void DrawBBox(const glm::vec3 & cur_min, const glm::vec3 & cur_max) {

        glColor3f(1, 0, 0);

        glBegin(GL_LINES);
        glVertex3f(cur_min.x, cur_min.y, cur_min.z);
        glVertex3f(cur_max.x, cur_min.y, cur_min.z);
        glEnd();

        glBegin(GL_LINES);
        glVertex3f(cur_min.x, cur_max.y, cur_min.z);
        glVertex3f(cur_max.x, cur_max.y, cur_min.z);
        glEnd();

        glBegin(GL_LINES);
        glVertex3f(cur_min.x, cur_min.y, cur_max.z);
        glVertex3f(cur_max.x, cur_min.y, cur_max.z);
        glEnd();

        glBegin(GL_LINES);
        glVertex3f(cur_min.x, cur_max.y, cur_max.z);
        glVertex3f(cur_max.x, cur_max.y, cur_max.z);
        glEnd();


        glColor3f(0, 1, 0);

        glBegin(GL_LINES);
        glVertex3f(cur_min.x, cur_min.y, cur_min.z);
        glVertex3f(cur_min.x, cur_max.y, cur_min.z);
        glEnd();

        glBegin(GL_LINES);
        glVertex3f(cur_max.x, cur_min.y, cur_min.z);
        glVertex3f(cur_max.x, cur_max.y, cur_min.z);
        glEnd();

        glBegin(GL_LINES);
        glVertex3f(cur_min.x, cur_min.y, cur_max.z);
        glVertex3f(cur_min.x, cur_max.y, cur_max.z);
        glEnd();

        glBegin(GL_LINES);
        glVertex3f(cur_max.x, cur_min.y, cur_max.z);
        glVertex3f(cur_max.x, cur_max.y, cur_max.z);
        glEnd();


        glColor3f(0, 0, 1);

        glBegin(GL_LINES);
        glVertex3f(cur_min.x, cur_min.y, cur_min.z);
        glVertex3f(cur_min.x, cur_min.y, cur_max.z);
        glEnd();

        glBegin(GL_LINES);
        glVertex3f(cur_min.x, cur_max.y, cur_min.z);
        glVertex3f(cur_min.x, cur_max.y, cur_max.z);
        glEnd();

        glBegin(GL_LINES);
        glVertex3f(cur_max.x, cur_min.y, cur_min.z);
        glVertex3f(cur_max.x, cur_min.y, cur_max.z);
        glEnd();

        glBegin(GL_LINES);
        glVertex3f(cur_max.x, cur_max.y, cur_min.z);
        glVertex3f(cur_max.x, cur_max.y, cur_max.z);
        glEnd();
}

void VisualHelpers::PrepareAxes() {

        const size_t COLOR_OFFSET = 3;

        std::vector<float> vVertices((3 + 3) * 6);

        vVertices[0 * VERT_SIZE + COLOR_OFFSET] = 1.0f;
        vVertices[1 * VERT_SIZE] = 1.0f;
        vVertices[1 * VERT_SIZE + COLOR_OFFSET] = 1.0f;

        vVertices[2 * VERT_SIZE + COLOR_OFFSET + 1] = 1.0f;
        vVertices[3 * VERT_SIZE + 1] = 1.0f;
        vVertices[3 * VERT_SIZE + COLOR_OFFSET + 1] = 1.0f;

        vVertices[4 * VERT_SIZE + COLOR_OFFSET + 2] = 1.0f;
        vVertices[5 * VERT_SIZE + 2] = 1.0f;
        vVertices[5 * VERT_SIZE + COLOR_OFFSET + 2] = 1.0f;

        glGenVertexArrays(1, &local_axes_vao);
        glBindVertexArray(local_axes_vao);

        uint32_t vbo_id;
        glGenBuffers(1, &vbo_id);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
        glBufferData(GL_ARRAY_BUFFER,
                     vVertices.size() * sizeof(float),
                     &vVertices[0],
                     GL_STATIC_DRAW);

        auto itLocation = mAttributeLocation.find("Position");
        if (itLocation == mAttributeLocation.end()) {
                glDeleteVertexArrays(1, &local_axes_vao);
                glDeleteBuffers(1, &vbo_id);
                throw(std::runtime_error("unknown vertex attribute name: 'Position'"));
        }

        glVertexAttribPointer(itLocation->second,
                        3,
                        GL_FLOAT,
                        false,
                        VERT_SIZE * sizeof(float),
                        (const void *)0);
        glEnableVertexAttribArray(itLocation->second);

        itLocation = mAttributeLocation.find("Color");
        if (itLocation == mAttributeLocation.end()) {
                glDeleteVertexArrays(1, &local_axes_vao);
                glDeleteBuffers(1, &vbo_id);
                throw(std::runtime_error("unknown vertex attribute name: 'Color'"));
        }

        glVertexAttribPointer(itLocation->second,
                        3,
                        GL_FLOAT,
                        false,
                        VERT_SIZE * sizeof(float),
                        (const void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(itLocation->second);

        glBindVertexArray(0);
        glDeleteBuffers(1, &vbo_id);
}

void VisualHelpers::UpdateBBox(const BoundingBox & oBBox) {

        static std::vector<float> vVertices(VERT_SIZE * 24);

        const glm::vec3 & vMin = oBBox.Min();
        const glm::vec3 & vMax = oBBox.Max();

        std::array<glm::vec3, 8> vPoints = {
                glm::vec3(vMax.x, vMin.y, vMin.z),
                glm::vec3(vMin.x, vMin.y, vMin.z),
                glm::vec3(vMin.x, vMax.y, vMin.z),
                glm::vec3(vMax.x, vMax.y, vMin.z),
                glm::vec3(vMax.x, vMin.y, vMax.z),
                glm::vec3(vMin.x, vMin.y, vMax.z),
                glm::vec3(vMin.x, vMax.y, vMax.z),
                glm::vec3(vMax.x, vMax.y, vMax.z)
        };

        glm::vec3 vColor(1, 0, 0);
        size_t cur_vert = 0;

        auto UpdateVert = [&cur_vert](glm::vec3 & vPoint, glm::vec3 & vColor) {

                vVertices[cur_vert * VERT_SIZE + 0] = vPoint.x;
                vVertices[cur_vert * VERT_SIZE + 1] = vPoint.y;
                vVertices[cur_vert * VERT_SIZE + 2] = vPoint.z;

                vVertices[cur_vert * VERT_SIZE + 3] = vColor.x;
                vVertices[cur_vert * VERT_SIZE + 4] = vColor.y;
                vVertices[cur_vert * VERT_SIZE + 5] = vColor.z;

                ++cur_vert;
        };

        UpdateVert(vPoints[0], vColor);
        UpdateVert(vPoints[1], vColor);

        UpdateVert(vPoints[2], vColor);
        UpdateVert(vPoints[3], vColor);

        UpdateVert(vPoints[4], vColor);
        UpdateVert(vPoints[5], vColor);

        UpdateVert(vPoints[6], vColor);
        UpdateVert(vPoints[7], vColor);

        vColor = glm::vec3(0, 1, 0);

        UpdateVert(vPoints[0], vColor);
        UpdateVert(vPoints[3], vColor);

        UpdateVert(vPoints[1], vColor);
        UpdateVert(vPoints[2], vColor);

        UpdateVert(vPoints[4], vColor);
        UpdateVert(vPoints[7], vColor);

        UpdateVert(vPoints[5], vColor);
        UpdateVert(vPoints[6], vColor);

        vColor = glm::vec3(0, 0, 1);

        UpdateVert(vPoints[0], vColor);
        UpdateVert(vPoints[4], vColor);

        UpdateVert(vPoints[1], vColor);
        UpdateVert(vPoints[5], vColor);

        UpdateVert(vPoints[2], vColor);
        UpdateVert(vPoints[6], vColor);

        UpdateVert(vPoints[3], vColor);
        UpdateVert(vPoints[7], vColor);

        glBindBuffer(GL_ARRAY_BUFFER, bbox_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     vVertices.size() * sizeof(float),
                     &vVertices[0],
                     GL_DYNAMIC_DRAW);
}

void VisualHelpers::PrepareBBox() {

        BoundingBox oTmpBox(glm::vec3(-1), glm::vec3(1));

        glGenVertexArrays(1, &bbox_vao);
        glBindVertexArray(bbox_vao);

        glGenBuffers(1, &bbox_vbo);
        UpdateBBox(oTmpBox);

        auto itLocation = mAttributeLocation.find("Position");
        if (itLocation == mAttributeLocation.end()) {
                glDeleteVertexArrays(1, &bbox_vao);
                glDeleteBuffers(1, &bbox_vbo);
                throw(std::runtime_error("unknown vertex attribute name: 'Position'"));
        }

        glVertexAttribPointer(itLocation->second,
                        3,
                        GL_FLOAT,
                        false,
                        VERT_SIZE * sizeof(float),
                        (const void *)0);
        glEnableVertexAttribArray(itLocation->second);

        itLocation = mAttributeLocation.find("Color");
        if (itLocation == mAttributeLocation.end()) {
                glDeleteVertexArrays(1, &bbox_vao);
                glDeleteBuffers(1, &bbox_vbo);
                throw(std::runtime_error("unknown vertex attribute name: 'Color'"));
        }

        glVertexAttribPointer(itLocation->second,
                        3,
                        GL_FLOAT,
                        false,
                        VERT_SIZE * sizeof(float),
                        (const void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(itLocation->second);

        glBindVertexArray(0);

}

VisualHelpers::VisualHelpers() {

        PrepareAxes();
        PrepareBBox();

        //TODO rewrite path handling after switching on global app settings
        pShader  = CreateResource<SE::ShaderProgram>(GetSystem<Config>().sResourceDir + "shader_program/simple_color.sesp");

}

VisualHelpers::~VisualHelpers() noexcept {

        glDeleteVertexArrays(1, &bbox_vao);
        glDeleteBuffers(1, &bbox_vbo);

        glDeleteVertexArrays(1, &local_axes_vao);
}

void VisualHelpers::DrawLocalAxes() {

        GetSystem<GraphicsState>().SetShaderProgram(pShader);
        GetSystem<GraphicsState>().DrawArrays(local_axes_vao, GL_LINES, 0, 6);
}

void VisualHelpers::DrawBBox(const BoundingBox & oBBox) {

        UpdateBBox(oBBox);

        GetSystem<GraphicsState>().SetShaderProgram(pShader);
        GetSystem<GraphicsState>().DrawArrays(bbox_vao, GL_LINES, 0, 24);
}

} //namespace HELPERS

} //namespace SE

